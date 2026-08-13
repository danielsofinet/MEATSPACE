#include "Character/MsCharacter.h"

#include "Combat/MsWeaponComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

AMsCharacter::AMsCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	Weapon = CreateDefaultSubobject<UMsWeaponComponent>(TEXT("Weapon"));
}

void AMsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	// Direct key binding for now. Movement/look still come from the Blueprint's Enhanced Input
	// graph, which we are not touching yet. When we build the full input layer we will replace
	// this with a proper IA_Fire Input Action asset - that is a binary asset Daniel creates in
	// the editor, so it waits until the mechanic is worth committing to.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMsCharacter::OnFirePressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMsCharacter::OnFireReleased);
}

void AMsCharacter::OnFirePressed()
{
	if (Weapon)
	{
		Weapon->StartFire();
	}
}

void AMsCharacter::OnFireReleased()
{
	if (Weapon)
	{
		Weapon->StopFire();
	}
}
