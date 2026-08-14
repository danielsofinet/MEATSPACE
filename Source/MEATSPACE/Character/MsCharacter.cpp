#include "Character/MsCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/MsGrenadeComponent.h"
#include "Combat/MsHealthComponent.h"
#include "Combat/MsMeleeComponent.h"
#include "Combat/MsWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/MsInteractable.h"
#include "World/MsNpc.h"

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

	HealthComponent = CreateDefaultSubobject<UMsHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 100.0f;
	HealthComponent->Health = 100.0f;

	// Partial protection, not immunity: 60% of each hit goes to the shield, 40% still lands
	// on health. Regenerates after a few seconds out of contact.
	HealthComponent->MaxShield = 60.0f;
	HealthComponent->Shield = 60.0f;
	HealthComponent->ShieldAbsorbFraction = 0.6f;
	HealthComponent->ShieldRegenDelay = 3.0f;
	HealthComponent->ShieldRegenRate = 14.0f;

	Grenade = CreateDefaultSubobject<UMsGrenadeComponent>(TEXT("Grenade"));
}

void AMsCharacter::BeginPlay()
{
	Super::BeginPlay();

	bSwordUnlocked = bStartWithSword;
	bGunUnlocked = bStartWithGun;

	// Shield is off by default and granted by the story. Applied here rather than in the
	// constructor so a designer changing StartingMaxShield does not have to reason about
	// component construction order.
	if (HealthComponent)
	{
		HealthComponent->MaxShield = StartingMaxShield;
		HealthComponent->Shield = StartingMaxShield;
	}

	if (HasAuthority())
	{
		// Never start holding a weapon we do not have.
		if (IsWeaponUnlocked(StartingSlot))
		{
			ActiveSlot = StartingSlot;
		}
		else if (bSwordUnlocked)
		{
			ActiveSlot = EMsWeaponSlot::Sword;
		}
		else if (bGunUnlocked)
		{
			ActiveSlot = EMsWeaponSlot::Gun;
		}
	}

	// The boom and camera live on the Blueprint (they came from the Third Person template),
	// so we adopt them rather than creating our own.
	CachedCameraBoom = FindComponentByClass<USpringArmComponent>();
	CachedFollowCamera = FindComponentByClass<UCameraComponent>();

	DesiredCameraYaw = CameraYaw;
	DesiredCameraPitch = CameraPitch;

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &AMsCharacter::HandleHealthChanged);
		HealthComponent->OnDeath.AddDynamic(this, &AMsCharacter::HandleDeath);
	}

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

	float ActiveYawSensitivity = IsAiming() ? AimYawSensitivity : YawSensitivity;
	float ActivePitchSensitivity = IsAiming() ? AimPitchSensitivity : PitchSensitivity;

	// Narrower FOV means the same mouse movement sweeps more world across the screen. Scaling
	// by the FOV ratio keeps the on-screen movement consistent at any zoom level, rather than
	// the camera becoming twitchy the moment you aim.
	if (bScaleSensitivityWithFOV && CameraFOV > KINDA_SMALL_NUMBER && CurrentFOV > 0.0f)
	{
		const float FOVRatio = FMath::Clamp(CurrentFOV / CameraFOV, 0.05f, 1.0f);
		const float Scale = FMath::Lerp(1.0f, FOVRatio, FMath::Clamp(FOVSensitivityStrength, 0.0f, 1.0f));

		ActiveYawSensitivity *= Scale;
		ActivePitchSensitivity *= Scale;
	}

	DesiredCameraYaw = FRotator::NormalizeAxis(DesiredCameraYaw + YawDelta * ActiveYawSensitivity);

	// Control pitch goes negative looking down; our CameraPitch goes positive.
	const float MinPitch = FMath::Min(PitchMin, PitchMax);
	const float MaxPitch = FMath::Max(PitchMin, PitchMax);
	DesiredCameraPitch = FMath::Clamp(DesiredCameraPitch - PitchDelta * ActivePitchSensitivity, MinPitch, MaxPitch);

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

	// Hit flash fades out on its own.
	if (DamageFlashAlpha > 0.0f)
	{
		DamageFlashAlpha = FMath::Max(0.0f, DamageFlashAlpha - DeltaSeconds / FMath::Max(DamageFlashDuration, 0.05f));
	}

	UpdateFocusedInteractable();

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

	// ADS FOV and distance no longer have hotkeys - G is the grenade now, and those two values
	// are settled. Edit them in the Blueprint's Class Defaults if they need changing.

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

void AMsCharacter::HandleHealthChanged(float NewHealth, float Delta)
{
	// Delta is negative when hurt. Fires on server and clients alike, so the feedback below
	// is guarded to whoever is actually looking through this character's camera.
	if (Delta >= 0.0f)
	{
		return;
	}

	if (IsLocallyControlled())
	{
		DamageFlashAlpha = 1.0f;
		AddCameraShake(HurtShake);
	}
}

void AMsCharacter::HandleDeath(AActor* DeadActor)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// Stop moving, stop colliding, stop being shootable. The actor stays alive so the camera
	// rig and the Blueprint's setup survive - destroying and respawning the pawn would mean
	// rebuilding all of it, and would break the reparented Blueprint's state.
	SetActorEnableCollision(false);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetHiddenInGame(true);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMsCharacter::Respawn, RespawnDelay, false);
	}
}

void AMsCharacter::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	// Back to a player start rather than dying in place, so a respawn is not instantly
	// swarmed by whatever just killed you.
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		if (AActor* StartSpot = GameMode->FindPlayerStart(GetController()))
		{
			SetActorLocationAndRotation(StartSpot->GetActorLocation(), StartSpot->GetActorRotation());
		}
	}

	if (HealthComponent)
	{
		HealthComponent->ResetHealth();
	}

	bIsDead = false;

	SetActorEnableCollision(true);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetHiddenInGame(false);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	DamageFlashAlpha = 0.0f;
	ShakeTrauma = 0.0f;
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
		FString::Printf(TEXT("ADS FOVx %.2f   DISTx %.2f%s"),
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

	// Scroll swaps weapons. Camera zoom is still available via ZoomIn/ZoomOut for a future
	// keybind, it just no longer owns the wheel - swapping mid-fight matters more.
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMsCharacter::OnCycleWeapon);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMsCharacter::OnCycleWeapon);

	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AMsCharacter::OnThrowGrenade);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMsCharacter::OnInteract);

	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AMsCharacter::OnToggleTuning);
}

void AMsCharacter::ZoomIn()
{
	CameraDistance = FMath::Clamp(CameraDistance - ZoomStep, MinCameraDistance, MaxCameraDistance);
}

void AMsCharacter::ZoomOut()
{
	CameraDistance = FMath::Clamp(CameraDistance + ZoomStep, MinCameraDistance, MaxCameraDistance);
}

void AMsCharacter::OnCycleWeapon()
{
	// Only two weapons, so either scroll direction just toggles. When there are more, this
	// becomes a proper indexed cycle that respects direction.
	EquipSlot(ActiveSlot == EMsWeaponSlot::Gun ? EMsWeaponSlot::Sword : EMsWeaponSlot::Gun);
}

void AMsCharacter::OnThrowGrenade()
{
	if (Grenade)
	{
		Grenade->ThrowGrenade();
	}
}

bool AMsCharacter::IsWeaponUnlocked(EMsWeaponSlot Slot) const
{
	return Slot == EMsWeaponSlot::Sword ? bSwordUnlocked : bGunUnlocked;
}

void AMsCharacter::UnlockWeapon(EMsWeaponSlot Slot, bool bEquipImmediately)
{
	if (Slot == EMsWeaponSlot::Sword)
	{
		bSwordUnlocked = true;
	}
	else
	{
		bGunUnlocked = true;
	}

	// Equip it if asked, or if it is the only thing we own - being handed a sword and still
	// holding nothing would be a strange moment.
	if (bEquipImmediately || ActiveSlot == Slot || !IsWeaponUnlocked(ActiveSlot))
	{
		EquipSlot(Slot);
	}

	ShowWeaponFeedback();
}

void AMsCharacter::UnlockShield(float NewMaxShield)
{
	if (!HealthComponent)
	{
		return;
	}

	HealthComponent->MaxShield = FMath::Max(NewMaxShield, 0.0f);

	// Arrive at full. Being handed a shield and having to wait for it to charge would be an
	// anticlimax.
	HealthComponent->Shield = HealthComponent->MaxShield;

	// The component only ticks when there is a shield to regenerate.
	HealthComponent->SetComponentTickEnabled(HasAuthority() && HealthComponent->HasShield());
}

void AMsCharacter::BeginDialogue(AMsNpc* Npc)
{
	ActiveDialogue = Npc;

	// Stop any attack in progress - talking to grandpa while spraying gunfire would be a look.
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AMsCharacter::EndDialogue()
{
	ActiveDialogue = nullptr;
}

void AMsCharacter::UpdateFocusedInteractable()
{
	FocusedInteractable = nullptr;

	const FVector MyLocation = GetActorLocation();
	float BestDistanceSq = FMath::Square(InteractSearchRadius);

	for (const TWeakObjectPtr<AMsInteractable>& Entry : AMsInteractable::GetAllInteractables())
	{
		AMsInteractable* Candidate = Entry.Get();
		if (!Candidate || !Candidate->CanInteract())
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), MyLocation);

		// Each interactable declares its own reach, so a big object can be talked to from
		// further away than a small one.
		const float ReachSq = FMath::Square(FMath::Min(Candidate->GetInteractRadius(), InteractSearchRadius));
		if (DistanceSq > ReachSq || DistanceSq > BestDistanceSq)
		{
			continue;
		}

		BestDistanceSq = DistanceSq;
		FocusedInteractable = Candidate;
	}
}

void AMsCharacter::OnInteract()
{
	// Mid-conversation, the interact key advances the dialogue rather than starting a new one.
	if (ActiveDialogue)
	{
		ActiveDialogue->Interact(this);
		return;
	}

	if (FocusedInteractable)
	{
		FocusedInteractable->Interact(this);
	}
}

void AMsCharacter::OnAttackPressed()
{
	// No weapon, or busy talking. Both should silently do nothing rather than swing at air.
	if (!HasAnyWeapon() || ActiveDialogue)
	{
		return;
	}

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
	if (ActiveSlot == NewSlot || !IsWeaponUnlocked(NewSlot))
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

		// Lock state is printed too, so "why can I still equip the gun" is answerable at a
		// glance instead of by reading code.
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()), 3.0f,
			bSword ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("EQUIPPED: %s      unlocked: sword %s / gun %s"),
				bSword ? TEXT("SWORD") : TEXT("GUN"),
				bSwordUnlocked ? TEXT("YES") : TEXT("no"),
				bGunUnlocked ? TEXT("YES") : TEXT("no")));
	}
}
