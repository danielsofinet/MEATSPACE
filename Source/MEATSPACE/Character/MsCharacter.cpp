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

bool AMsCharacter::IsAiming() const
{
	// Right mouse only zooms with the gun out. The sword will want its own right-mouse
	// behaviour later - a block or a dash - so this stays weapon-gated from the start.
	return bAllowAimZoom && bAimHeld && ActiveSlot == EMsWeaponSlot::Gun;
}

EMsCameraMode AMsCharacter::GetEffectiveCameraMode() const
{
	// Aiming temporarily takes over the camera scheme.
	if (bAimSwitchesToOrbit && IsAiming())
	{
		return EMsCameraMode::Orbit;
	}

	return CameraMode;
}

void AMsCharacter::SeedOrbitTracking()
{
	// Start the delta tracking from where the camera already is, so the first orbit frame
	// produces zero movement instead of a jump.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(FRotator(-CameraPitch, CameraYaw, 0.0f));
	}

	LastControlYaw = CameraYaw;
	LastControlPitch = -CameraPitch;
}

void AMsCharacter::OnAimPressed()
{
	YawBeforeAiming = CameraYaw;
	PitchBeforeAiming = CameraPitch;
	bAimHeld = true;

	// Seed immediately rather than waiting for ApplyCameraMode, so the very first frame of
	// aiming cannot snap the view.
	SeedOrbitTracking();
}

void AMsCharacter::OnAimReleased()
{
	bAimHeld = false;

	if (bRestoreYawAfterAiming)
	{
		CameraYaw = YawBeforeAiming;
	}

	// The base pitch is an art decision - a temporary aim should not permanently change it.
	if (bRestorePitchAfterAiming && CameraMode != EMsCameraMode::Orbit)
	{
		CameraPitch = PitchBeforeAiming;
	}
}

void AMsCharacter::ApplyCameraSettings()
{
	if (!bUseFixedCamera)
	{
		return;
	}

	// Ease FOV and boom length toward their targets so aiming zooms smoothly. Initialised on
	// the first frame so the camera does not lerp up from zero when play starts.
	const float TargetFOV = IsAiming() ? CameraFOV * AimFOVMultiplier : CameraFOV;
	const float TargetDistance = IsAiming() ? CameraDistance * AimDistanceMultiplier : CameraDistance;

	if (CurrentFOV <= 0.0f || CurrentDistance <= 0.0f)
	{
		CurrentFOV = TargetFOV;
		CurrentDistance = TargetDistance;
	}
	else
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, AimTransitionSpeed);
		CurrentDistance = FMath::FInterpTo(CurrentDistance, TargetDistance, DeltaSeconds, AimTransitionSpeed);
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

		CachedCameraBoom->TargetArmLength = CurrentDistance;
		CachedCameraBoom->SocketOffset = FVector::ZeroVector;

		// Without this the boom collides with level geometry behind the character and snaps
		// the camera in - unusable for a top-down rig where the boom is always inside walls.
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

void AMsCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	TickCameraTuning(DeltaSeconds);

	// Re-apply every frame so live edits in the editor take effect while playing.
	ApplyCameraSettings();

	// Must run after ApplyCameraSettings - it layers on top of the base rotation it sets.
	TickCameraJuice(DeltaSeconds);

	TickCameraFollow(DeltaSeconds);

	ApplyCameraMode();

	APlayerController* PC = Cast<APlayerController>(GetController());
	const EMsCameraMode Mode = GetEffectiveCameraMode();

	if (bUseFixedCamera && PC)
	{
		if (Mode == EMsCameraMode::Orbit)
		{
			// The Blueprint's Look input drives control rotation with the mouse. We read it as
			// a DELTA rather than an absolute so the movement can be scaled - reading it
			// absolutely would make sensitivity impossible to change from here.
			const FRotator ControlRotation = PC->GetControlRotation();

			const float YawDelta = FRotator::NormalizeAxis(ControlRotation.Yaw - LastControlYaw);
			const float PitchDelta = FRotator::NormalizeAxis(ControlRotation.Pitch) - LastControlPitch;

			const float YawSensitivity = IsAiming() ? AimYawSensitivity : OrbitYawSensitivity;
			CameraYaw = FRotator::NormalizeAxis(CameraYaw + YawDelta * YawSensitivity);

			// Control pitch goes negative looking down; our CameraPitch goes positive. Clamped
			// so the mouse gives vertical life without destroying the forced perspective.
			const float MinPitch = FMath::Min(OrbitPitchMin, OrbitPitchMax);
			const float MaxPitch = FMath::Max(OrbitPitchMin, OrbitPitchMax);
			CameraPitch = FMath::Clamp(CameraPitch - PitchDelta * OrbitPitchSensitivity, MinPitch, MaxPitch);

			// Write the clamped result back so control rotation never drifts away from the
			// camera, and so the next frame's delta is purely new mouse movement.
			PC->SetControlRotation(FRotator(-CameraPitch, CameraYaw, 0.0f));
			LastControlYaw = CameraYaw;
			LastControlPitch = -CameraPitch;
		}
		else
		{
			// Pin the control rotation to the camera's yaw. The Blueprint's movement input is
			// relative to control rotation, so this is what keeps WASD aligned to the screen
			// instead of drifting with the mouse.
			PC->SetControlRotation(FRotator(0.0f, CameraYaw, 0.0f));
		}
	}

	// Facing: toward the cursor, except in Orbit where the character faces where the camera
	// looks - that is what makes a centre-screen crosshair mean anything.
	if (Mode == EMsCameraMode::Orbit)
	{
		const FRotator Desired(0.0f, CameraYaw, 0.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, FaceCursorSpeed));
	}
	else if (bFaceCursor)
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

				// FollowAim: the camera drifts around to sit behind your aim, giving the view
				// motion without taking the mouse away from aiming.
				if (Mode == EMsCameraMode::FollowAim)
				{
					const float TargetYaw = ToAim.Rotation().Yaw;
					CameraYaw = FMath::FInterpTo(CameraYaw, TargetYaw, DeltaSeconds, FollowAimSpeed);
				}
			}
		}
	}
}

void AMsCharacter::ApplyCameraMode()
{
	const EMsCameraMode Mode = GetEffectiveCameraMode();

	if (bModeApplied && LastAppliedMode == Mode)
	{
		return;
	}

	const bool bAimDriven = bAimSwitchesToOrbit && IsAiming();

	LastAppliedMode = Mode;
	bModeApplied = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (Mode == EMsCameraMode::Orbit)
		{
			// Capture the mouse properly. Hiding the cursor is not enough - an uncaptured
			// cursor still hits the edge of the screen, at which point it stops producing
			// movement and the camera appears to stick and judder against the boundary.
			FInputModeGameOnly InputMode;
			InputMode.SetConsumeCaptureMouseDown(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;

			SeedOrbitTracking();
		}
		else
		{
			// Cursor modes: visible, and locked inside the viewport so aiming cannot wander
			// onto a second monitor mid-fight.
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}

	// Peek is meaningless in Orbit - the camera is already pointing where you look.
	CurrentPeekOffset = FVector::ZeroVector;

	// Only announce deliberate mode changes, not the constant flicker of aiming in and out.
	if (GEngine && !bAimDriven)
	{
		const TCHAR* ModeName =
			Mode == EMsCameraMode::Orbit ? TEXT("ORBIT (mouse turns camera, centre-screen aim)") :
			Mode == EMsCameraMode::FollowAim ? TEXT("FOLLOW AIM (cursor aims, camera drifts to follow)") :
			TEXT("FIXED (cursor aims, yaw locked, Q/E to rotate)");

		GEngine->AddOnScreenDebugMessage(9008, 3.0f, FColor::Magenta,
			FString::Printf(TEXT("CAMERA MODE: %s   [C to cycle]"), ModeName));
	}
}

void AMsCharacter::OnCycleCameraMode()
{
	switch (CameraMode)
	{
	case EMsCameraMode::Fixed:		CameraMode = EMsCameraMode::Orbit; break;
	case EMsCameraMode::Orbit:		CameraMode = EMsCameraMode::FollowAim; break;
	default:						CameraMode = EMsCameraMode::Fixed; break;
	}
}

void AMsCharacter::TickCameraTuning(float DeltaSeconds)
{
	if (!bCameraTuningMode || !bUseFixedCamera)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Held keys rather than presses, so values sweep smoothly and you can feel the change
	// happening rather than stepping through it.
	if (PC->IsInputKeyDown(EKeys::I))
	{
		CameraPitch = FMath::Clamp(CameraPitch + PitchAdjustRate * DeltaSeconds, 5.0f, 89.0f);
	}
	if (PC->IsInputKeyDown(EKeys::K))
	{
		CameraPitch = FMath::Clamp(CameraPitch - PitchAdjustRate * DeltaSeconds, 5.0f, 89.0f);
	}

	if (PC->IsInputKeyDown(EKeys::U))
	{
		CameraFOV = FMath::Clamp(CameraFOV - FOVAdjustRate * DeltaSeconds, 5.0f, 120.0f);
	}
	if (PC->IsInputKeyDown(EKeys::O))
	{
		CameraFOV = FMath::Clamp(CameraFOV + FOVAdjustRate * DeltaSeconds, 5.0f, 120.0f);
	}

	if (PC->IsInputKeyDown(EKeys::J))
	{
		CameraYaw -= YawAdjustRate * DeltaSeconds;
	}
	if (PC->IsInputKeyDown(EKeys::L))
	{
		CameraYaw += YawAdjustRate * DeltaSeconds;
	}

	if (PC->IsInputKeyDown(EKeys::N))
	{
		MousePeekStrength = FMath::Clamp(MousePeekStrength - PeekAdjustRate * DeltaSeconds, 0.0f, 1.0f);
	}
	if (PC->IsInputKeyDown(EKeys::M))
	{
		MousePeekStrength = FMath::Clamp(MousePeekStrength + PeekAdjustRate * DeltaSeconds, 0.0f, 1.0f);
	}

	// Aim-zoom tuning. Hold right mouse while adjusting to see it applied live.
	//
	// Letters only. Punctuation keys sit in different physical places across keyboard
	// layouts - on Swedish QWERTY the US bracket and quote keys are Å, ¨, Ö and Ä - whereas
	// letter keys are in the same place everywhere.
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

	ShowCameraReadout();
}

void AMsCharacter::TickCameraFollow(float DeltaSeconds)
{
	if (!bUseFixedCamera || !CachedCameraBoom)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());

	// Q / E swing the whole view around the character. Pointless in Orbit, where the mouse
	// already owns the yaw and this would just fight it.
	if (PC && bAllowCameraRotate && GetEffectiveCameraMode() != EMsCameraMode::Orbit)
	{
		if (PC->IsInputKeyDown(EKeys::Q))
		{
			CameraYaw -= CameraRotateRate * DeltaSeconds;
		}
		if (PC->IsInputKeyDown(EKeys::E))
		{
			CameraYaw += CameraRotateRate * DeltaSeconds;
		}
	}

	// Lean toward the cursor, driven by where the cursor sits ON SCREEN.
	//
	// The previous version used the world point under the cursor, which broke at low camera
	// pitch: near the horizon that point is thousands of units away, so the lean saturated
	// its clamp and lurched. Screen space is linear - centre is no lean, edge is full lean,
	// regardless of camera angle or what the cursor happens to be over.
	FVector DesiredPeek = FVector::ZeroVector;

	// No peek in Orbit - the camera already points where you are looking.
	if (MousePeekStrength > 0.0f && PC && UsesCursorAim())
	{
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PC->GetViewportSize(ViewportX, ViewportY);

		float MouseX = 0.0f;
		float MouseY = 0.0f;

		if (ViewportX > 0 && ViewportY > 0 && PC->GetMousePosition(MouseX, MouseY))
		{
			// -1..1 from screen centre.
			float NormX = (MouseX / ViewportX) * 2.0f - 1.0f;
			float NormY = (MouseY / ViewportY) * 2.0f - 1.0f;

			// Deadzone, rescaled so the lean still reaches full strength at the screen edge
			// instead of being permanently short by the deadzone amount.
			auto ApplyDeadzone = [this](float Value)
			{
				const float Magnitude = FMath::Abs(Value);
				if (Magnitude <= PeekDeadzone)
				{
					return 0.0f;
				}
				const float Rescaled = (Magnitude - PeekDeadzone) / FMath::Max(1.0f - PeekDeadzone, KINDA_SMALL_NUMBER);
				return FMath::Sign(Value) * FMath::Min(Rescaled, 1.0f);
			};

			NormX = ApplyDeadzone(NormX);
			NormY = ApplyDeadzone(NormY);

			// Convert screen axes into world directions using the camera's yaw. Screen Y grows
			// downward, so it maps to negative camera-forward.
			const FRotator YawOnly(0.0f, CameraYaw, 0.0f);
			const FVector CameraRight = YawOnly.RotateVector(FVector::RightVector);
			const FVector CameraForward = YawOnly.RotateVector(FVector::ForwardVector);

			DesiredPeek =
				CameraRight * NormX * PeekHorizontalScale +
				CameraForward * -NormY * PeekVerticalScale;

			DesiredPeek *= MaxPeekDistance * MousePeekStrength;

			// Hold the camera steadier while aiming down sights.
			if (IsAiming())
			{
				DesiredPeek *= AimPeekMultiplier;
			}
		}
	}

	CurrentPeekOffset = FMath::VInterpTo(CurrentPeekOffset, DesiredPeek, DeltaSeconds, PeekLagSpeed);
	CachedCameraBoom->TargetOffset = CurrentPeekOffset;
}

void AMsCharacter::TickCameraJuice(float DeltaSeconds)
{
	if (!bUseFixedCamera || !CachedCameraBoom)
	{
		return;
	}

	SwayTime += DeltaSeconds;

	// Trauma drains continuously. Repeated hits stack rather than restarting an animation,
	// which is why a burst of gunfire builds into a rumble instead of stuttering.
	ShakeTrauma = FMath::Max(0.0f, ShakeTrauma - ShakeDecayRate * DeltaSeconds);

	// Squaring makes small trauma nearly invisible and large trauma hit hard.
	const float Shake = ShakeTrauma * ShakeTrauma;

	FRotator Offset = FRotator::ZeroRotator;

	if (bCameraSway)
	{
		// Sway harder the faster you are moving, so running feels physical.
		float SpeedAlpha = 0.0f;
		if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			const float MaxSpeed = FMath::Max(Movement->MaxWalkSpeed, 1.0f);
			SpeedAlpha = FMath::Clamp(GetVelocity().Size2D() / MaxSpeed, 0.0f, 1.0f);
		}

		const float Amplitude = SwayAmplitude * (1.0f + SpeedAlpha * SwayMoveBoost);

		// Two different frequencies so it never traces a repeating circle.
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
	// Only the player looking through this camera should feel it.
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

	// Debug only - fixed message keys so the lines update in place instead of scrolling.
	GEngine->AddOnScreenDebugMessage(9001, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("PITCH  %.1f      [I raise / K lower]"), CameraPitch));
	GEngine->AddOnScreenDebugMessage(9002, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("FOV    %.1f      [U narrow / O wide]"), CameraFOV));
	GEngine->AddOnScreenDebugMessage(9003, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("DIST   %.0f      [scroll wheel]"), CameraDistance));
	GEngine->AddOnScreenDebugMessage(9004, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("YAW    %.1f      [J left / L right, or Q/E]"), CameraYaw));
	GEngine->AddOnScreenDebugMessage(9005, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("PEEK   %.2f      [N less / M more]"), MousePeekStrength));
	GEngine->AddOnScreenDebugMessage(9007, 0.0f, IsAiming() ? FColor::Cyan : FColor::Silver,
		FString::Printf(TEXT("ADS FOVx %.2f  [G less / H more]   DISTx %.2f  [V less / B more]%s"),
			AimFOVMultiplier, AimDistanceMultiplier, IsAiming() ? TEXT("   << AIMING") : TEXT("")));
	GEngine->AddOnScreenDebugMessage(9006, 0.0f, FColor::Green,
		TEXT("--- CAMERA TUNING (P to hide) ---"));
}

void AMsCharacter::OnToggleTuning()
{
	bCameraTuningMode = !bCameraTuningMode;

	if (!bCameraTuningMode && GEngine)
	{
		// Clear the readout lines so they do not linger once tuning is off.
		for (int32 Key = 9001; Key <= 9007; ++Key)
		{
			GEngine->RemoveOnScreenDebugMessage(Key);
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

	if (UsesCursorAim())
	{
		if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
		{
			return false;
		}
	}
	else
	{
		// Orbit mode: the mouse is turning the camera, so aim comes from screen centre.
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PC->GetViewportSize(ViewportX, ViewportY);

		if (ViewportX <= 0 || ViewportY <= 0)
		{
			return false;
		}

		if (!PC->DeprojectScreenPositionToWorld(ViewportX * 0.5f, ViewportY * 0.5f, RayOrigin, RayDirection))
		{
			return false;
		}
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

	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AMsCharacter::OnToggleTuning);

	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMsCharacter::OnAimPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMsCharacter::OnAimReleased);

	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AMsCharacter::OnCycleCameraMode);
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
