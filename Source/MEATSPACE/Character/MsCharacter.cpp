#include "Character/MsCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/MsMeleeComponent.h"
#include "Combat/MsWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"

AMsCharacter::AMsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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

	// The boom and camera live on the Blueprint (they came from the Third Person template),
	// so we adopt them rather than creating our own.
	CachedCameraBoom = FindComponentByClass<USpringArmComponent>();
	CachedFollowCamera = FindComponentByClass<UCameraComponent>();

	ApplyCameraSettings();

	if (bUseFixedCamera)
	{
		// The character turns to face the cursor, so nothing else may drive its rotation.
		bUseControllerRotationYaw = false;
		bUseControllerRotationPitch = false;
		bUseControllerRotationRoll = false;

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = false;
		}
	}

	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// Free-moving cursor: this is the aiming device now.
			PC->bShowMouseCursor = true;
		}

		ShowWeaponFeedback();
	}
}

void AMsCharacter::ApplyCameraSettings()
{
	if (!bUseFixedCamera)
	{
		return;
	}

	if (CachedCameraBoom)
	{
		// Absolute rotation - the boom must not inherit anything from the pawn or controller,
		// or the "fixed" camera would swing around as the character turns to face the cursor.
		CachedCameraBoom->bUsePawnControlRotation = false;
		CachedCameraBoom->bInheritPitch = false;
		CachedCameraBoom->bInheritYaw = false;
		CachedCameraBoom->bInheritRoll = false;
		CachedCameraBoom->SetUsingAbsoluteRotation(true);
		CachedCameraBoom->SetWorldRotation(FRotator(-CameraPitch, CameraYaw, 0.0f));

		CachedCameraBoom->TargetArmLength = CameraDistance;
		CachedCameraBoom->SocketOffset = FVector::ZeroVector;
		CachedCameraBoom->TargetOffset = FVector::ZeroVector;

		// Without this the boom collides with level geometry behind the character and snaps
		// the camera in - unusable for a top-down rig where the boom is always inside walls.
		CachedCameraBoom->bDoCollisionTest = false;

		CachedCameraBoom->bEnableCameraLag = CameraLagSpeed > 0.0f;
		CachedCameraBoom->CameraLagSpeed = CameraLagSpeed;
	}

	if (CachedFollowCamera)
	{
		CachedFollowCamera->SetFieldOfView(CameraFOV);
		CachedFollowCamera->bUsePawnControlRotation = false;
	}
}

void AMsCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	// Re-apply every frame so live edits in the editor take effect while playing.
	ApplyCameraSettings();

	if (bUseFixedCamera)
	{
		// Pin the control rotation to the camera's yaw. The Blueprint's movement input is
		// relative to control rotation, so this is what keeps WASD aligned to the screen
		// instead of drifting with the mouse.
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetControlRotation(FRotator(0.0f, CameraYaw, 0.0f));
		}
	}

	if (bFaceCursor)
	{
		FVector AimPoint;
		if (ComputeAimPoint(AimPoint))
		{
			FVector ToAim = AimPoint - GetActorLocation();
			ToAim.Z = 0.0f;

			if (!ToAim.IsNearlyZero())
			{
				const FRotator Desired(0.0f, ToAim.Rotation().Yaw, 0.0f);
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, FaceCursorSpeed));
			}
		}
	}
}

bool AMsCharacter::ComputeAimPoint(FVector& OutAimPoint) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	const FVector RayEnd = RayOrigin + RayDirection * AimTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsAimTrace), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	// Tracing rather than projecting onto a ground plane is what lets the cursor pick out
	// flying clankers - the ray passes through them on its way down.
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, RayOrigin, RayEnd, ECC_Visibility, Params))
	{
		OutAimPoint = Hit.ImpactPoint;
		return true;
	}

	OutAimPoint = RayEnd;
	return true;
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

	// Direct key bindings for now. Movement still comes from the Blueprint's Enhanced Input
	// graph, which we are not touching. These become proper Input Action assets once the
	// mechanics are worth committing to - those are editor-authored binary assets.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMsCharacter::OnAttackPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMsCharacter::OnAttackReleased);

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMsCharacter::OnSelectSword);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMsCharacter::OnSelectGun);

	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMsCharacter::OnZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMsCharacter::OnZoomOut);
}

void AMsCharacter::OnZoomIn()
{
	CameraDistance = FMath::Clamp(CameraDistance - ZoomStep, MinCameraDistance, MaxCameraDistance);
	ApplyCameraSettings();
}

void AMsCharacter::OnZoomOut()
{
	CameraDistance = FMath::Clamp(CameraDistance + ZoomStep, MinCameraDistance, MaxCameraDistance);
	ApplyCameraSettings();
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
