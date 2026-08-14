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
	if (const AMsCharacter* Character = Cast<AMsCharacter>(GetOwningPawn()))
	{
		bSwordEquipped = (Character->GetActiveSlot() == EMsWeaponSlot::Sword);
	}

	if (bSwordEquipped && !bShowWithSword)
	{
		return;
	}

	// Under the forced-perspective rig, aiming is cursor-driven - screen centre is the ground
	// at the player's feet. So the marker follows the mouse, which is where shots actually go.
	float CentreX = Canvas->ClipX * 0.5f;
	float CentreY = Canvas->ClipY * 0.5f;

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
