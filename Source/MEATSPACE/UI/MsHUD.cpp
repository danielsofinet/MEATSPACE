#include "UI/MsHUD.h"

#include "Character/MsCharacter.h"
#include "Combat/MsCombatTypes.h"
#include "Combat/MsHealthComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void AMsHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const AMsCharacter* OwnerCharacter = Cast<AMsCharacter>(GetOwningPawn());

	// Flash first so it sits behind the rest of the HUD.
	DrawDamageFlash(OwnerCharacter);
	DrawHealthBar(OwnerCharacter);

	// Dead players do not get a crosshair - there is nothing to aim.
	if (OwnerCharacter && OwnerCharacter->IsDead())
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
