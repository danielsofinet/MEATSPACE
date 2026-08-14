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

namespace
{
	/** Maps 0..1 dullness onto an interpolation speed. 0 is near-instant, 1 is heavy. */
	float DullnessToFollowSpeed(float Dullness)
	{
		return FMath::Lerp(45.0f, 2.5f, FMath::Clamp(Dullness, 0.0f, 1.0f));
	}

	/** Angle interpolation that survives the +/-180 wrap. */
	float InterpAngle(float Current, float Target, float DeltaSeconds, float Speed)
	{
		const float Delta = FRotator::NormalizeAxis(Target - Current);
		const float Alpha = FMath::Clamp(DeltaSeconds * Speed, 0.0f, 1.0f);
		return FRotator::NormalizeAxis(Current + Delta * Alpha);
	}
}

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

	DesiredCameraYaw = CameraYaw;
	DesiredCameraPitch = CameraPitch;

	if (bDriveCameraRig)
	{
		// The character faces where the camera looks, so nothing else may drive its rotation.
		bUseControllerRotationYaw = false;
		bUseControllerRotationPitch = false;
		bUseControllerRotationRoll = false;

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = false;
		}
	}

	ApplyCameraSettings();

	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// Capture the mouse. Merely hiding the cursor is not enough - an uncaptured
			// cursor still reaches the edge of the screen, stops producing movement, and the
			// camera appears to stick and judder against the boundary.
			FInputModeGameOnly InputMode;
			InputMode.SetConsumeCaptureMouseDown(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;

			SeedLookTracking();
		}

		ShowWeaponFeedback();
	}
}

void AMsCharacter::SeedLookTracking()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(FRotator(-DesiredCameraPitch, DesiredCameraYaw, 0.0f));
	}

	LastControlYaw = DesiredCameraYaw;
	LastControlPitch = -DesiredCameraPitch;
}

bool AMsCharacter::IsAiming() const
{
	// The sword will want its own right-mouse behaviour later - a block or a dash - so this
	// stays weapon-gated from the start.
	return bAllowAimZoom && bAimHeld && ActiveSlot == EMsWeaponSlot::Gun;
}

void AMsCharacter::OnAimPressed()
{
	bAimHeld = true;
}

void AMsCharacter::OnAimReleased()
{
	bAimHeld = false;
}

void AMsCharacter::ApplyCameraSettings()
{
	if (!bDriveCameraRig)
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	// Ease FOV and boom length toward their targets so aiming zooms smoothly.
	const float TargetFOV = IsAiming() ? CameraFOV * AimFOVMultiplier : CameraFOV;
	const float TargetDistance = IsAiming() ? CameraDistance * AimDistanceMultiplier : CameraDistance;

	// The reticle moves between its hip and aim positions on the SAME easing as the zoom.
	// Snapping it instantly is what made the crosshair appear to jump ahead of the transition.
	const FVector2D TargetCrosshair = IsAiming() ? AimCrosshairOffset : HipCrosshairOffset;

	if (CurrentFOV <= 0.0f || CurrentDistance <= 0.0f || !bCrosshairInitialised)
	{
		CurrentFOV = TargetFOV;
		CurrentDistance = TargetDistance;
		CurrentCrosshairOffset = TargetCrosshair;
		bCrosshairInitialised = true;
	}
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, AimTransitionSpeed);
		CurrentDistance = FMath::FInterpTo(CurrentDistance, TargetDistance, DeltaSeconds, AimTransitionSpeed);
		CurrentCrosshairOffset = FMath::Vector2DInterpTo(
			CurrentCrosshairOffset, TargetCrosshair, DeltaSeconds, AimTransitionSpeed);
	}

	if (CachedCameraBoom)
	{
		// Absolute rotation - the boom must not inherit anything from the pawn, or it would
		// swing around as the character turns.
		CachedCameraBoom->bUsePawnControlRotation = false;
		CachedCameraBoom->bInheritPitch = false;
		CachedCameraBoom->bInheritYaw = false;
		CachedCameraBoom->bInheritRoll = false;
		CachedCameraBoom->SetUsingAbsoluteRotation(true);
		CachedCameraBoom->SetWorldRotation(FRotator(-CameraPitch, CameraYaw, 0.0f));

		CachedCameraBoom->TargetArmLength = CurrentDistance;

		// Keep the camera above the floor when looking up.
		//
		// The boom trails behind whatever it points at, so tilting up swings the camera DOWN
		// and back - at this arm length that buries it underground long before the pitch limit
		// is reached. Clamping the look-up angle would "fix" it by making flying clankers
		// unhittable, so instead we raise the boom's pivot by exactly the shortfall.
		const float PitchRadians = FMath::DegreesToRadians(CameraPitch);
		const float CameraHeightAbovePivot = FMath::Sin(PitchRadians) * CurrentDistance;
		const float RequiredLift = FMath::Max(0.0f, MinCameraHeight - CameraHeightAbovePivot);

		CachedCameraBoom->TargetOffset = FVector(0.0f, 0.0f, RequiredLift);

		// Shoulder shift, eased. SocketOffset is applied in the boom's own space, so it moves
		// the character sideways in frame without changing where the camera points.
		const FVector TargetSocketOffset = IsAiming() ? AimSocketOffset : HipSocketOffset;
		CachedCameraBoom->SocketOffset = FMath::VInterpTo(
			CachedCameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, AimTransitionSpeed);

		// A top-down boom lives inside geometry; collision testing would snap the camera in.
		CachedCameraBoom->bDoCollisionTest = false;

		CachedCameraBoom->bEnableCameraLag = CameraLagSpeed > 0.0f;
		CachedCameraBoom->CameraLagSpeed = CameraLagSpeed;
	}

	if (CachedFollowCamera)
	{
		CachedFollowCamera->SetFieldOfView(CurrentFOV);
		CachedFollowCamera->bUsePawnControlRotation = false;
	}
}

void AMsCharacter::TickMouseLook(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !bDriveCameraRig)
	{
		return;
	}

	// The Blueprint's Look input drives control rotation with the mouse. We consume it as a
	// DELTA rather than reading it absolutely, which is what makes sensitivity scalable and
	// lets us clamp pitch without fighting the input system.
	const FRotator ControlRotation = PC->GetControlRotation();

	const float YawDelta = FRotator::NormalizeAxis(ControlRotation.Yaw - LastControlYaw);
	const float PitchDelta = FRotator::NormalizeAxis(ControlRotation.Pitch) - LastControlPitch;

	const float ActiveYawSensitivity = IsAiming() ? AimYawSensitivity : YawSensitivity;
	DesiredCameraYaw = FRotator::NormalizeAxis(DesiredCameraYaw + YawDelta * ActiveYawSensitivity);

	// Control pitch goes negative looking down; our CameraPitch goes positive.
	const float MinPitch = FMath::Min(PitchMin, PitchMax);
	const float MaxPitch = FMath::Max(PitchMin, PitchMax);
	DesiredCameraPitch = FMath::Clamp(DesiredCameraPitch - PitchDelta * PitchSensitivity, MinPitch, MaxPitch);

	// Write the clamped result back so control rotation never drifts away from the camera,
	// and so next frame's delta is purely new mouse movement.
	PC->SetControlRotation(FRotator(-DesiredCameraPitch, DesiredCameraYaw, 0.0f));
	LastControlYaw = DesiredCameraYaw;
	LastControlPitch = -DesiredCameraPitch;

	// Dullness: the camera trails the mouse rather than being welded to it. Aiming uses a
	// lower value because precision wants a rigid camera.
	const float FollowSpeed = DullnessToFollowSpeed(IsAiming() ? AimCameraDullness : CameraDullness);
	CameraYaw = InterpAngle(CameraYaw, DesiredCameraYaw, DeltaSeconds, FollowSpeed);
	CameraPitch = FMath::FInterpTo(CameraPitch, DesiredCameraPitch, DeltaSeconds, FollowSpeed);
}

void AMsCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	TickMouseLook(DeltaSeconds);
	TickCameraTuning(DeltaSeconds);

	// Re-applied every frame so live edits take effect while playing.
	ApplyCameraSettings();

	// Must run after ApplyCameraSettings - it layers on top of the base rotation it sets.
	TickCameraJuice(DeltaSeconds);

	// The body follows where the camera looks. That is what makes a fixed reticle mean
	// anything: the character is always pointing at what you are about to shoot.
	const FRotator DesiredFacing(0.0f, CameraYaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredFacing, DeltaSeconds, CharacterTurnSpeed));
}

bool AMsCharacter::ComputeAimPoint(FVector& OutAimPoint) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);

	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	// Deproject through the reticle's actual screen position. Anything else would make the
	// crosshair lie about where shots land.
	const FVector2D Offset = GetCrosshairScreenOffset();
	const float ScreenX = ViewportX * 0.5f + Offset.X * ViewportX * 0.5f;
	const float ScreenY = ViewportY * 0.5f + Offset.Y * ViewportY * 0.5f;

	FVector RayOrigin;
	FVector RayDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, RayOrigin, RayDirection))
	{
		return false;
	}

	const FVector RayEnd = RayOrigin + RayDirection * 25000.0f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsAimTrace), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	// Tracing rather than projecting onto a ground plane is what lets the reticle pick out
	// flying clankers - the ray passes through them on its way out.
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, RayOrigin, RayEnd, ECC_Visibility, Params))
	{
		OutAimPoint = Hit.ImpactPoint;
		return true;
	}

	OutAimPoint = RayEnd;
	return true;
}

void AMsCharacter::TickCameraTuning(float DeltaSeconds)
{
	if (!bCameraTuningMode || !bDriveCameraRig)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Held keys rather than presses, so values sweep smoothly and the change can be felt.
	// Letters, digits and arrows only - punctuation sits in different physical places across
	// keyboard layouts.
	if (PC->IsInputKeyDown(EKeys::U))
	{
		CameraFOV = FMath::Clamp(CameraFOV - FOVAdjustRate * DeltaSeconds, 5.0f, 120.0f);
	}
	if (PC->IsInputKeyDown(EKeys::O))
	{
		CameraFOV = FMath::Clamp(CameraFOV + FOVAdjustRate * DeltaSeconds, 5.0f, 120.0f);
	}

	// Dullness of whichever state we are in right now.
	float& Dullness = IsAiming() ? AimCameraDullness : CameraDullness;
	if (PC->IsInputKeyDown(EKeys::N))
	{
		Dullness = FMath::Clamp(Dullness - 0.35f * DeltaSeconds, 0.0f, 1.0f);
	}
	if (PC->IsInputKeyDown(EKeys::M))
	{
		Dullness = FMath::Clamp(Dullness + 0.35f * DeltaSeconds, 0.0f, 1.0f);
	}

	// Look-up limit. Negative values let the camera see into the sky.
	if (PC->IsInputKeyDown(EKeys::J))
	{
		PitchMin = FMath::Clamp(PitchMin - 12.0f * DeltaSeconds, -80.0f, 89.0f);
	}
	if (PC->IsInputKeyDown(EKeys::L))
	{
		PitchMin = FMath::Clamp(PitchMin + 12.0f * DeltaSeconds, -80.0f, 89.0f);
	}

	// Shoulder offset of the current state.
	FVector& Socket = ActiveSocketOffset();
	if (PC->IsInputKeyDown(EKeys::I))
	{
		Socket.Y += 60.0f * DeltaSeconds;
	}
	if (PC->IsInputKeyDown(EKeys::K))
	{
		Socket.Y -= 60.0f * DeltaSeconds;
	}

	if (PC->IsInputKeyDown(EKeys::G))
	{
		AimFOVMultiplier = FMath::Clamp(AimFOVMultiplier - 0.35f * DeltaSeconds, 0.1f, 2.0f);
	}
	if (PC->IsInputKeyDown(EKeys::H))
	{
		AimFOVMultiplier = FMath::Clamp(AimFOVMultiplier + 0.35f * DeltaSeconds, 0.1f, 2.0f);
	}

	if (PC->IsInputKeyDown(EKeys::V))
	{
		AimDistanceMultiplier = FMath::Clamp(AimDistanceMultiplier - 0.35f * DeltaSeconds, 0.1f, 2.0f);
	}
	if (PC->IsInputKeyDown(EKeys::B))
	{
		AimDistanceMultiplier = FMath::Clamp(AimDistanceMultiplier + 0.35f * DeltaSeconds, 0.1f, 2.0f);
	}

	// Arrows move whichever reticle is currently in force.
	FVector2D& Reticle = ActiveCrosshairOffset();
	const float ReticleRate = 0.35f;
	if (PC->IsInputKeyDown(EKeys::Left))
	{
		Reticle.X = FMath::Clamp(Reticle.X - ReticleRate * DeltaSeconds, -1.0f, 1.0f);
	}
	if (PC->IsInputKeyDown(EKeys::Right))
	{
		Reticle.X = FMath::Clamp(Reticle.X + ReticleRate * DeltaSeconds, -1.0f, 1.0f);
	}
	if (PC->IsInputKeyDown(EKeys::Up))
	{
		Reticle.Y = FMath::Clamp(Reticle.Y - ReticleRate * DeltaSeconds, -1.0f, 1.0f);
	}
	if (PC->IsInputKeyDown(EKeys::Down))
	{
		Reticle.Y = FMath::Clamp(Reticle.Y + ReticleRate * DeltaSeconds, -1.0f, 1.0f);
	}

	ShowCameraReadout();
}

void AMsCharacter::TickCameraJuice(float DeltaSeconds)
{
	if (!bDriveCameraRig || !CachedCameraBoom)
	{
		return;
	}

	SwayTime += DeltaSeconds;

	// Trauma drains continuously. Repeated hits stack rather than restarting an animation,
	// which is why sustained fire builds into a rumble instead of stuttering.
	ShakeTrauma = FMath::Max(0.0f, ShakeTrauma - ShakeDecayRate * DeltaSeconds);

	// Squaring makes small trauma nearly invisible and large trauma hit hard.
	const float Shake = ShakeTrauma * ShakeTrauma;

	FRotator Offset = FRotator::ZeroRotator;

	if (bCameraSway)
	{
		float SpeedAlpha = 0.0f;
		if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			const float MaxSpeed = FMath::Max(Movement->MaxWalkSpeed, 1.0f);
			SpeedAlpha = FMath::Clamp(GetVelocity().Size2D() / MaxSpeed, 0.0f, 1.0f);
		}

		const float Amplitude = SwayAmplitude * (1.0f + SpeedAlpha * SwayMoveBoost);

		// Unrelated frequencies so it never traces a repeating circle.
		Offset.Pitch += FMath::Sin(SwayTime * SwayFrequency) * Amplitude;
		Offset.Yaw += FMath::Cos(SwayTime * SwayFrequency * 0.73f) * Amplitude;
		Offset.Roll += FMath::Sin(SwayTime * SwayFrequency * 0.41f) * Amplitude * 0.5f;
	}

	if (Shake > KINDA_SMALL_NUMBER)
	{
		// Perlin rather than sine: sine reads as a wobble, noise reads as an impact.
		Offset.Pitch += FMath::PerlinNoise1D(SwayTime * ShakeFrequency) * ShakeAngle * Shake;
		Offset.Yaw += FMath::PerlinNoise1D((SwayTime + 137.0f) * ShakeFrequency) * ShakeAngle * Shake;
		Offset.Roll += FMath::PerlinNoise1D((SwayTime + 291.0f) * ShakeFrequency) * ShakeRoll * Shake;
	}

	CachedCameraBoom->SetWorldRotation(FRotator(-CameraPitch, CameraYaw, 0.0f) + Offset);
}

void AMsCharacter::AddCameraShake(float Trauma)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	ShakeTrauma = FMath::Clamp(ShakeTrauma + Trauma, 0.0f, 1.0f);
}

void AMsCharacter::OnWeaponFired()
{
	AddCameraShake(FireShake);
}

void AMsCharacter::OnWeaponHit()
{
	AddCameraShake(GunHitShake);
}

void AMsCharacter::OnSwordSwing()
{
	AddCameraShake(SwordSwingShake);
}

void AMsCharacter::OnSwordHit()
{
	AddCameraShake(SwordHitShake);
}

void AMsCharacter::ShowCameraReadout() const
{
	if (!GEngine)
	{
		return;
	}

	const bool bAiming = IsAiming();
	const FVector2D Reticle = bAiming ? AimCrosshairOffset : HipCrosshairOffset;
	const FVector Socket = bAiming ? AimSocketOffset : HipSocketOffset;
	const float Dullness = bAiming ? AimCameraDullness : CameraDullness;

	// Fixed message keys so the lines update in place instead of scrolling.
	GEngine->AddOnScreenDebugMessage(9001, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("FOV     %.1f      [U narrow / O wide]"), CameraFOV));
	GEngine->AddOnScreenDebugMessage(9002, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("DIST    %.0f      [scroll wheel]"), CameraDistance));
	GEngine->AddOnScreenDebugMessage(9003, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("PITCH   %.1f  (limits %.0f to %.0f)   [J raise / L lower look-up limit]"),
			CameraPitch, PitchMin, PitchMax));
	GEngine->AddOnScreenDebugMessage(9004, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("DULLNESS %.2f     [N snappier / M duller]"), Dullness));
	GEngine->AddOnScreenDebugMessage(9005, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("RETICLE x %.3f  y %.3f   [arrows]     SHOULDER Y %.0f   [I out / K in]"),
			Reticle.X, Reticle.Y, Socket.Y));
	GEngine->AddOnScreenDebugMessage(9007, 0.0f, bAiming ? FColor::Cyan : FColor::Silver,
		FString::Printf(TEXT("ADS FOVx %.2f  [G / H]     DISTx %.2f  [V / B]%s"),
			AimFOVMultiplier, AimDistanceMultiplier, bAiming ? TEXT("   << AIMING") : TEXT("")));
	GEngine->AddOnScreenDebugMessage(9006, 0.0f, FColor::Green,
		TEXT("--- CAMERA TUNING (P to hide) ---"));
}

void AMsCharacter::OnToggleTuning()
{
	bCameraTuningMode = !bCameraTuningMode;

	if (!bCameraTuningMode && GEngine)
	{
		for (int32 Key = 9001; Key <= 9009; ++Key)
		{
			GEngine->RemoveOnScreenDebugMessage(Key);
		}
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

	// Direct key bindings for now. Movement and look still come from the Blueprint's Enhanced
	// Input graph. These become proper Input Action assets once the mechanics are worth
	// committing to - those are editor-authored binary assets.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMsCharacter::OnAttackPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMsCharacter::OnAttackReleased);

	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMsCharacter::OnAimPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMsCharacter::OnAimReleased);

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMsCharacter::OnSelectSword);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMsCharacter::OnSelectGun);

	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMsCharacter::OnZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMsCharacter::OnZoomOut);

	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AMsCharacter::OnToggleTuning);
}

void AMsCharacter::OnZoomIn()
{
	CameraDistance = FMath::Clamp(CameraDistance - ZoomStep, MinCameraDistance, MaxCameraDistance);
}

void AMsCharacter::OnZoomOut()
{
	CameraDistance = FMath::Clamp(CameraDistance + ZoomStep, MinCameraDistance, MaxCameraDistance);
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
