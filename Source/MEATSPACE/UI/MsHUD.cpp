#include "UI/MsHUD.h"

#include "Character/MsCharacter.h"
#include "Combat/MsCombatTypes.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void AMsHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
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
