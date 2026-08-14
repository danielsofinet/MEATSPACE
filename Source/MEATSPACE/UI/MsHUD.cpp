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

	// Two aiming schemes, two reticle positions:
	//   cursor modes - the marker follows the mouse, which is the aiming device
	//   orbit / ADS  - fixed position, offset from centre by the character's setting
	// Either way this must match the point ComputeAimPoint deprojects through, or the
	// crosshair would lie about where shots land.
	float CentreX = Canvas->ClipX * 0.5f;
	float CentreY = Canvas->ClipY * 0.5f;

	const AMsCharacter* Character = Cast<AMsCharacter>(GetOwningPawn());
	const bool bCursorAim = !Character || Character->UsesCursorAim();

	if (bCursorAim)
	{
		if (const APlayerController* PC = GetOwningPlayerController())
		{
			float MouseX = 0.0f;
			float MouseY = 0.0f;
			if (PC->GetMousePosition(MouseX, MouseY))
			{
				CentreX = MouseX;
				CentreY = MouseY;
			}
		}
	}
	else
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
