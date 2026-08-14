#include "Character/MsCharacter.h"

#include "Combat/MsMeleeComponent.h"
#include "Combat/MsWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"

AMsCharacter::AMsCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	Weapon = CreateDefaultSubobject<UMsWeaponComponent>(TEXT("Weapon"));
	Melee = CreateDefaultSubobject<UMsMeleeComponent>(TEXT("Melee"));
}

void AMsCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ActiveSlot = StartingSlot;
	}

	if (IsLocallyControlled())
	{
		ShowWeaponFeedback();
	}
}

void AMsCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMsCharacter, ActiveSlot);
}

void AMsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	// Direct key bindings for now. Movement/look still come from the Blueprint's Enhanced
	// Input graph, which we are not touching. These become proper Input Action assets once
	// the mechanics are worth committing to - those are editor-authored binary assets.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMsCharacter::OnAttackPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMsCharacter::OnAttackReleased);

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMsCharacter::OnSelectSword);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMsCharacter::OnSelectGun);
}

void AMsCharacter::OnAttackPressed()
{
	switch (ActiveSlot)
	{
	case EMsWeaponSlot::Sword:
		if (Melee)
		{
			Melee->StartSwing();
		}
		break;

	case EMsWeaponSlot::Gun:
	default:
		if (Weapon)
		{
			Weapon->StartFire();
		}
		break;
	}
}

void AMsCharacter::OnAttackReleased()
{
	// Only the gun cares about release - the sword is a discrete swing, not a hold.
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AMsCharacter::OnSelectSword()
{
	EquipSlot(EMsWeaponSlot::Sword);
}

void AMsCharacter::OnSelectGun()
{
	EquipSlot(EMsWeaponSlot::Gun);
}

void AMsCharacter::EquipSlot(EMsWeaponSlot NewSlot)
{
	if (ActiveSlot == NewSlot)
	{
		return;
	}

	// Never leave the gun firing into a swap.
	if (Weapon)
	{
		Weapon->StopFire();
	}

	// Apply locally straight away so the swap feels instant regardless of ping...
	ActiveSlot = NewSlot;
	ShowWeaponFeedback();

	// ...then let the server confirm. On a listen-server host this runs inline.
	ServerEquipSlot(NewSlot);
}

void AMsCharacter::ServerEquipSlot_Implementation(EMsWeaponSlot NewSlot)
{
	if (ActiveSlot == NewSlot)
	{
		return;
	}

	if (Weapon)
	{
		Weapon->StopFire();
	}

	ActiveSlot = NewSlot;
}

void AMsCharacter::OnRep_ActiveSlot()
{
	if (IsLocallyControlled())
	{
		ShowWeaponFeedback();
	}
}

void AMsCharacter::ShowWeaponFeedback() const
{
	// Debug only - never seen by a player, so a plain FString is fine. Anything a player
	// actually reads must be FText in a String Table (see CLAUDE.md).
	if (GEngine)
	{
		const bool bSword = (ActiveSlot == EMsWeaponSlot::Sword);
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()), 2.0f,
			bSword ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("EQUIPPED: %s"), bSword ? TEXT("SWORD  [1]") : TEXT("GUN  [2]")));
	}
}
