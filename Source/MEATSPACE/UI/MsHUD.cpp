#include "UI/MsHUD.h"

#include "Character/MsCharacter.h"
#include "Combat/MsCombatTypes.h"
#include "Combat/MsGrenadeComponent.h"
#include "Combat/MsHealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Game/MsCutsceneSubsystem.h"
#include "Game/MsObjectiveSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "World/MsInteractable.h"
#include "World/MsNpc.h"

void AMsHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const AMsCharacter* OwnerCharacter = Cast<AMsCharacter>(GetOwningPawn());

	// A cutscene owns the whole screen. Drawing the health bar or crosshair over it would
	// break the one thing a cutscene is for.
	if (UWorld* World = GetWorld())
	{
		if (const UMsCutsceneSubsystem* Cutscenes = World->GetSubsystem<UMsCutsceneSubsystem>())
		{
			if (Cutscenes->IsPlaying())
			{
				const float Alpha = Cutscenes->GetOverlayAlpha();
				DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, Alpha), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

				const FString Line = Cutscenes->GetCurrentLine().ToString();
				if (!Line.IsEmpty())
				{
					float TextWidth = 0.0f;
					float TextHeight = 0.0f;
					GetTextSize(Line, TextWidth, TextHeight, nullptr, StoryTextScale);

					DrawText(Line, FLinearColor(1.0f, 1.0f, 1.0f, Alpha),
						(Canvas->ClipX - TextWidth) * 0.5f, Canvas->ClipY * 0.5f,
						nullptr, StoryTextScale);
				}

				return;
			}
		}
	}

	// Flash first so it sits behind the rest of the HUD.
	DrawDamageFlash(OwnerCharacter);
	DrawHealthBar(OwnerCharacter);
	DrawObjective();
	DrawInteractionPrompt(OwnerCharacter);
	DrawDialogue(OwnerCharacter);

	// Dead players do not get a crosshair - there is nothing to aim.
	if (OwnerCharacter && OwnerCharacter->IsDead())
	{
		return;
	}

	// Neither do unarmed ones, or players mid-conversation. A crosshair implies you can
	// shoot, and in both cases you cannot.
	if (OwnerCharacter && (!OwnerCharacter->HasAnyWeapon() || OwnerCharacter->GetActiveDialogue()))
	{
		return;
	}

	// Which weapon is out decides the colour, and whether we draw at all.
	bool bSwordEquipped = false;
	if (const AMsCharacter* EquippedCheck = Cast<AMsCharacter>(GetOwningPawn()))
	{
		bSwordEquipped = (EquippedCheck->GetActiveSlot() == EMsWeaponSlot::Sword);
	}

	if (bSwordEquipped && !bShowWithSword)
	{
		return;
	}

	// The reticle sits at a fixed screen position, offset from centre. This MUST match the
	// point ComputeAimPoint deprojects through, or the crosshair would lie about where shots
	// land. A dead-centre reticle is wrong for this camera: the boom points at the character,
	// so screen centre is the ground just behind him.
	float CentreX = Canvas->ClipX * 0.5f;
	float CentreY = Canvas->ClipY * 0.5f;

	if (const AMsCharacter* Character = Cast<AMsCharacter>(GetOwningPawn()))
	{
		const FVector2D Offset = Character->GetCrosshairScreenOffset();
		CentreX += Offset.X * Canvas->ClipX * 0.5f;
		CentreY += Offset.Y * Canvas->ClipY * 0.5f;
	}

	const float Half = DotSize * 0.5f;

	if (OutlineThickness > 0.0f)
	{
		const float OutlineHalf = Half + OutlineThickness;
		DrawRect(OutlineColor,
			CentreX - OutlineHalf, CentreY - OutlineHalf,
			OutlineHalf * 2.0f, OutlineHalf * 2.0f);
	}

	DrawRect(bSwordEquipped ? SwordDotColor : DotColor,
		CentreX - Half, CentreY - Half,
		DotSize, DotSize);
}

void AMsHUD::DrawHealthBar(const AMsCharacter* Character)
{
	if (!bShowHealthBar || !Character || !Canvas)
	{
		return;
	}

	const UMsHealthComponent* Health = Character->GetHealth();
	if (!Health)
	{
		return;
	}

	const float Percent = FMath::Clamp(Health->GetHealthPercent(), 0.0f, 1.0f);

	const float BarWidth = Canvas->ClipX * HealthBarWidthFraction;
	const float BarX = (Canvas->ClipX - BarWidth) * 0.5f;
	const float BarY = Canvas->ClipY - HealthBarBottomMargin - HealthBarHeight;

	// Backing plate, slightly oversized, so the bar reads against any background.
	const float Padding = 2.0f;
	DrawRect(HealthBarBackColor,
		BarX - Padding, BarY - Padding,
		BarWidth + Padding * 2.0f, HealthBarHeight + Padding * 2.0f);

	if (Percent > 0.0f)
	{
		// Green through to red as it drains, so low health is legible peripherally - you
		// should never have to look directly at the bar to know you are in trouble.
		const FLinearColor FillColor = FMath::Lerp(HealthLowColor, HealthHighColor, Percent);
		DrawRect(FillColor, BarX, BarY, BarWidth * Percent, HealthBarHeight);
	}

	// Grenade readiness, directly under the health bar. Drains as it recharges, so "can I
	// throw" is answerable at a glance without reading a number.
	if (const UMsGrenadeComponent* GrenadeComp = Character->GetGrenade())
	{
		const float Cooling = FMath::Clamp(GrenadeComp->GetCooldownFraction(), 0.0f, 1.0f);
		const float PipY = BarY + HealthBarHeight + ShieldBarGap;
		const float PipHeight = 6.0f;

		DrawRect(HealthBarBackColor,
			BarX - Padding, PipY - Padding,
			BarWidth + Padding * 2.0f, PipHeight + Padding * 2.0f);

		const float ReadyFraction = 1.0f - Cooling;
		if (ReadyFraction > 0.0f)
		{
			const FLinearColor PipColor = Cooling > 0.0f
				? FLinearColor(0.55f, 0.55f, 0.6f, 0.9f)
				: FLinearColor(1.0f, 0.85f, 0.25f, 0.95f);

			DrawRect(PipColor, BarX, PipY, BarWidth * ReadyFraction, PipHeight);
		}
	}

	// Shield above health. Separate bar rather than a shared one, because the two behave
	// differently - shield comes back on its own, health does not - and that difference is
	// the thing the player needs to read at a glance.
	if (Health->HasShield())
	{
		const float ShieldPercent = FMath::Clamp(Health->GetShieldPercent(), 0.0f, 1.0f);
		const float ShieldY = BarY - ShieldBarGap - ShieldBarHeight;

		DrawRect(HealthBarBackColor,
			BarX - Padding, ShieldY - Padding,
			BarWidth + Padding * 2.0f, ShieldBarHeight + Padding * 2.0f);

		if (ShieldPercent > 0.0f)
		{
			DrawRect(ShieldColor, BarX, ShieldY, BarWidth * ShieldPercent, ShieldBarHeight);
		}
	}
}

void AMsHUD::DrawObjective()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	const UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>();
	if (!Objectives || !Objectives->HasObjective())
	{
		return;
	}

	// FText is the source of truth; ToString happens only at the point of rendering. Anything
	// that stores or passes the text around keeps it as FText so it stays translatable.
	const FString Text = Objectives->GetObjectiveText().ToString();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, nullptr, StoryTextScale);

	DrawText(Text, ObjectiveColor,
		(Canvas->ClipX - TextWidth) * 0.5f, Canvas->ClipY * 0.06f,
		nullptr, StoryTextScale);
}

void AMsHUD::DrawInteractionPrompt(const AMsCharacter* Character)
{
	if (!Canvas || !Character || Character->GetActiveDialogue())
	{
		return;
	}

	const AMsInteractable* Interactable = Character->GetFocusedInteractable();
	if (!Interactable)
	{
		return;
	}

	// The key is not translated; the verb is. Formatting them together rather than
	// concatenating keeps word order translatable - some languages put the verb first.
	const FText Prompt = FText::Format(
		NSLOCTEXT("Meatspace.Interaction", "InteractPromptFormat", "[E]  {Action}"),
		Interactable->GetPrompt());

	const FString Text = Prompt.ToString();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, nullptr, StoryTextScale);

	DrawText(Text, PromptColor,
		(Canvas->ClipX - TextWidth) * 0.5f, Canvas->ClipY * 0.62f,
		nullptr, StoryTextScale);
}

void AMsHUD::DrawDialogue(const AMsCharacter* Character)
{
	if (!Canvas || !Character)
	{
		return;
	}

	const AMsNpc* Npc = Character->GetActiveDialogue();
	if (!Npc || !Npc->IsTalking())
	{
		return;
	}

	const float PanelHeight = Canvas->ClipY * DialogueHeightFraction;
	const float PanelY = Canvas->ClipY - PanelHeight;
	const float Margin = Canvas->ClipX * 0.08f;

	DrawRect(DialogueBackColor, 0.0f, PanelY, Canvas->ClipX, PanelHeight);

	DrawText(Npc->GetSpeakerName().ToString(), SpeakerColor,
		Margin, PanelY + PanelHeight * 0.18f, nullptr, StoryTextScale);

	DrawText(Npc->GetCurrentLine().ToString(), PromptColor,
		Margin, PanelY + PanelHeight * 0.45f, nullptr, StoryTextScale);

	const FString Continue = NSLOCTEXT("Meatspace.Dialogue", "ContinuePrompt", "[E]  Continue").ToString();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Continue, TextWidth, TextHeight, nullptr, StoryTextScale * 0.8f);

	DrawText(Continue, FLinearColor(0.7f, 0.7f, 0.75f, 0.9f),
		Canvas->ClipX - Margin - TextWidth, PanelY + PanelHeight * 0.75f,
		nullptr, StoryTextScale * 0.8f);
}

void AMsHUD::DrawDamageFlash(const AMsCharacter* Character)
{
	if (!Character || !Canvas)
	{
		return;
	}

	const float Flash = Character->GetDamageFlash();
	if (Flash <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Squared so the flash spikes and drops away fast rather than lingering as a red haze.
	const float Alpha = Flash * Flash * DamageFlashOpacity;
	DrawRect(FLinearColor(0.7f, 0.0f, 0.0f, Alpha), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
}
