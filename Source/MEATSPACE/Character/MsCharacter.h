#pragma once

#include "CoreMinimal.h"
#include "Combat/MsCombatTypes.h"
#include "GameFramework/Character.h"
#include "MsCharacter.generated.h"

class UMsWeaponComponent;
class UMsMeleeComponent;
class USpringArmComponent;
class UCameraComponent;

/**
 * How the camera is driven.
 *
 * The mouse can only do one job. Either it aims - cursor picks the target and the camera yaw
 * stays put - or it rotates the camera, in which case aiming has to fall back to a
 * centre-screen crosshair. There is no configuration where it does both.
 */
UENUM(BlueprintType)
enum class EMsCameraMode : uint8
{
	/** Yaw locked. Cursor aims. Q/E rotate manually. Best horde awareness. */
	Fixed		UMETA(DisplayName = "Fixed angle"),

	/** Mouse rotates the camera, classic third-person. Aim is centre-screen. */
	Orbit		UMETA(DisplayName = "Mouse orbit (TPS)"),

	/** Cursor aims, and the camera lazily swings to follow where you are aiming. */
	FollowAim	UMETA(DisplayName = "Follow aim")
};

/**
 * Base player character for MEATSPACE.
 *
 * Camera is a fixed forced-perspective rig, not a follow-cam: locked pitch and yaw, long
 * boom, narrow FOV. The narrow FOV is what sells it - it compresses depth so the scene reads
 * as near-isometric while still being a perspective camera, which keeps big skies and tall
 * geometry looking dramatic instead of flat.
 *
 * Because the camera looks down, screen centre is the ground at your feet, so aiming is
 * cursor-driven rather than centre-screen. The aim ray is deprojected from the mouse and
 * traced into the world, which means putting the cursor on a flying clanker aims at it -
 * a ground-plane projection could never hit anything airborne.
 */
UCLASS()
class MEATSPACE_API AMsCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMsCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Anti-air. Hitscan. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	UMsWeaponComponent* GetWeapon() const { return Weapon; }

	/** Anti-ground. Swept arc. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	UMsMeleeComponent* GetMelee() const { return Melee; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	EMsWeaponSlot GetActiveSlot() const { return ActiveSlot; }

	/** Swap weapons. Safe to call on client - replicates to the server. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Combat")
	void EquipSlot(EMsWeaponSlot NewSlot);

	/**
	 * World point under the mouse cursor, found by tracing the deprojected cursor ray.
	 * Returns false when there is no local player controller (i.e. on remote proxies).
	 */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Aim")
	bool ComputeAimPoint(FVector& OutAimPoint) const;

	/**
	 * Adds trauma to the camera. Trauma decays over time and drives the shake, so repeated
	 * hits stack into a bigger jolt instead of restarting a fixed animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Camera")
	void AddCameraShake(float Trauma);

	/** Called by the weapon and sword so the camera reacts to what you are doing. */
	void OnWeaponFired();
	void OnWeaponHit();
	void OnSwordSwing();
	void OnSwordHit();

	/** True while right mouse is held with the gun equipped. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	bool IsAiming() const;

	/** Spread multiplier the weapon should apply right now. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	float GetAimSpreadMultiplier() const { return IsAiming() ? AimSpreadMultiplier : 1.0f; }

	/** The mode actually in force this frame, accounting for the aim override. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Camera")
	EMsCameraMode GetEffectiveCameraMode() const;

	/** False in Orbit mode, where the mouse drives the camera and aiming is centre-screen. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	bool UsesCursorAim() const { return GetEffectiveCameraMode() != EMsCameraMode::Orbit; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsMeleeComponent> Melee;

	/** Which weapon is out. Replicated so other players can see it later. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlot, BlueprintReadOnly, Category = "Meatspace|Combat")
	EMsWeaponSlot ActiveSlot = EMsWeaponSlot::Gun;

	/** Which weapon the character starts the game holding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Combat")
	EMsWeaponSlot StartingSlot = EMsWeaponSlot::Gun;

	// --- Camera rig. All live-tunable; this is the section to play with. ---

	/** Take over the Blueprint's camera boom and drive it as a fixed rig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	bool bUseFixedCamera = true;

	/** Which camera scheme runs while NOT aiming. Cycle in game with C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	EMsCameraMode CameraMode = EMsCameraMode::Fixed;

	/**
	 * Holding right mouse switches to Orbit for as long as it is held.
	 *
	 * This is the resolution to "the mouse can only do one job": hipfire is twin-stick, with
	 * the cursor aiming and a wide stable view for reading the horde; aiming is third-person,
	 * with the mouse turning the camera and a centre-screen crosshair. You pick which job the
	 * mouse is doing, moment to moment.
	 *
	 * It also fixes the zoom: magnifying screen centre only helps if you are aiming at screen
	 * centre, which you are in Orbit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	bool bAimSwitchesToOrbit = true;

	/**
	 * Snap the camera yaw back to where it was before aiming. Off by default: letting the new
	 * angle persist means aiming doubles as a way to turn the camera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	bool bRestoreYawAfterAiming = false;

	/** FollowAim only: how fast the camera swings around to follow your aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "0.1"))
	float FollowAimSpeed = 2.5f;

	/**
	 * Orbit only: how far the mouse can tilt the camera. Clamped rather than free so the
	 * forced-perspective look survives - you get vertical life without being able to swing
	 * into a top-down or a ground-level view.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float OrbitPitchMin = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float OrbitPitchMax = 50.0f;

	/**
	 * Snap the pitch back to the base camera pitch when an aim ends. On by default: the base
	 * pitch is an art decision, so a temporary aim should not permanently change the look.
	 * Irrelevant when Orbit is the full-time mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit")
	bool bRestorePitchAfterAiming = true;

	/** Horizontal turn multiplier in Orbit. 1.0 is whatever the Blueprint's Look input gives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit", meta = (ClampMin = "0.05"))
	float OrbitYawSensitivity = 1.0f;

	/** Horizontal turn multiplier while aiming. Above 1 so ADS swings around faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit", meta = (ClampMin = "0.05"))
	float AimYawSensitivity = 1.7f;

	/** Vertical multiplier. Usually lower than horizontal - vertical tilt is more sensitive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Orbit", meta = (ClampMin = "0.05"))
	float OrbitPitchSensitivity = 0.7f;

	/**
	 * Downward tilt in degrees. Low values keep the horizon and sky in frame, which is what
	 * this game wants - dialled in by eye at 24.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float CameraPitch = 24.0f;

	/** World yaw the camera looks along. Rotate the whole view by changing this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	float CameraYaw = 0.0f;

	/** Boom length. Bigger = further out = more battlefield visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "100.0"))
	float CameraDistance = 3090.0f;

	/**
	 * Narrow FOV is what creates the forced-perspective look. 90 is a normal game camera;
	 * 30-45 flattens the scene toward isometric while keeping perspective depth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float CameraFOV = 40.6f;

	/** How lazily the camera follows. Lower = floatier, higher = glued to the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 9.0f;

	/** Scroll wheel zoom limits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MinCameraDistance = 2450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MaxCameraDistance = 4210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "10.0"))
	float ZoomStep = 160.0f;

	// --- Mouse-follow. The camera leans toward the cursor instead of staying centred. ---

	/**
	 * How far the camera leans toward the cursor, as a fraction of MaxPeekDistance when the
	 * cursor is at the screen edge. 0 disables it entirely.
	 *
	 * Driven by cursor position ON SCREEN, not by the world point under it. The world-point
	 * version felt wrong for a reason: at a low camera pitch, the point under the cursor can
	 * be thousands of units away near the horizon, so the lean pinned itself to the clamp and
	 * lurched. Screen space is linear and predictable - edge of screen is always full lean.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MousePeekStrength = 0.30f;

	/** Lean at full deflection, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0"))
	float MaxPeekDistance = 700.0f;

	/**
	 * Fraction of the screen around the centre where the camera does not move at all.
	 * Stops the view twitching while you make small aim corrections.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float PeekDeadzone = 0.15f;

	/** Scales the sideways lean independently. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0"))
	float PeekHorizontalScale = 1.0f;

	/**
	 * Scales the forward/back lean independently. Often worth keeping lower than horizontal -
	 * vertical camera drift is much more disorienting than sideways drift.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0"))
	float PeekVerticalScale = 0.6f;

	/** How lazily the lean follows the cursor. Lower = smoother, higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.1"))
	float PeekLagSpeed = 5.0f;

	/** Let Q and E swing the camera yaw around the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse")
	bool bAllowCameraRotate = true;

	/** Degrees per second while Q or E is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "1.0"))
	float CameraRotateRate = 90.0f;

	// --- Aim-down-sights. Hold right mouse with the gun out. ---

	/** Enables the right-mouse zoom. Only applies while the gun is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom")
	bool bAllowAimZoom = true;

	/** FOV multiplier while aiming. Kept mild - the look-ahead below does the real work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimFOVMultiplier = 0.72f;

	/** Boom length multiplier while aiming. Under 1 pulls the camera in toward the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimDistanceMultiplier = 0.88f;

	/** How fast the zoom eases in and out. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.5"))
	float AimTransitionSpeed = 8.0f;

	/**
	 * Mouse-follow multiplier while aiming. Deliberately ABOVE 1.
	 *
	 * Narrowing the FOV magnifies whatever is at screen centre, and at this camera pitch
	 * screen centre is the ground at your feet - so a plain zoom just gives you a close-up of
	 * the floor. Pushing the view further along your aim is what makes aiming show you the
	 * thing you are shooting at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float AimPeekMultiplier = 1.9f;

	/** Weapon spread is multiplied by this while aiming. Aiming should reward you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSpreadMultiplier = 0.25f;

	// --- Camera life. Idle sway plus reactive shake. ---

	/**
	 * Constant gentle drift so the camera never sits perfectly still. This is the single
	 * biggest difference between a camera that feels alive and one that feels like a
	 * screenshot - keep it small enough that you notice it only when it stops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice")
	bool bCameraSway = true;

	/** Sway size in degrees. Subtle: 0.2-0.6 is the useful range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayAmplitude = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayFrequency = 1.1f;

	/** Extra sway at full running speed, as a multiplier. Makes movement feel physical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayMoveBoost = 1.8f;

	/** Peak shake angle in degrees at full trauma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float ShakeAngle = 2.6f;

	/** Peak roll in degrees. Roll is what makes a shake feel like an impact rather than a nudge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float ShakeRoll = 1.6f;

	/** How jittery the shake is. Higher = sharper and more electric. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.1"))
	float ShakeFrequency = 20.0f;

	/** How fast trauma drains. Higher = snappier recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.1"))
	float ShakeDecayRate = 2.4f;

	/** Recoil per gunshot. Small - it fires eight times a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireShake = 0.10f;

	/** Extra trauma when a shot actually connects, so hits read differently from misses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GunHitShake = 0.10f;

	/**
	 * Trauma when a swing STARTS, whether or not it connects. Zero by design: swinging
	 * through empty air should not move the camera, or every whiff feels like a hit and
	 * the impact loses all its meaning. Raise it only if the sword should feel heavy to
	 * merely swing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordSwingShake = 0.0f;

	/** Trauma added when the blade actually connects. Should be clearly bigger than the swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordHitShake = 0.38f;

	// --- Live tuning. Hold the keys while playing; read the values off screen. ---

	/**
	 * Enables the tuning hotkeys and the on-screen readout. Toggle in game with P.
	 * Turn this off before shipping anything - it is a development tool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	bool bCameraTuningMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	float PitchAdjustRate = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	float YawAdjustRate = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	float FOVAdjustRate = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	float PeekAdjustRate = 0.35f;

	// --- Aiming ---

	/** How far the cursor ray reaches before giving up and aiming at empty space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim", meta = (ClampMin = "100.0"))
	float AimTraceDistance = 25000.0f;

	/** Turn the character to face the cursor. Twin-stick style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim")
	bool bFaceCursor = true;

	/** How fast the character snaps around to the cursor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim", meta = (ClampMin = "0.1"))
	float FaceCursorSpeed = 18.0f;

	UFUNCTION()
	void OnRep_ActiveSlot();

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(EMsWeaponSlot NewSlot);

	/** Pushes the camera properties above onto the Blueprint's boom and camera. */
	void ApplyCameraSettings();

	/** Reads the tuning hotkeys and nudges the camera values. */
	void TickCameraTuning(float DeltaSeconds);

	/** Leans the camera toward the cursor and handles Q/E rotation. */
	void TickCameraFollow(float DeltaSeconds);

	/** Layers sway and decaying shake on top of the fixed camera rotation. */
	void TickCameraJuice(float DeltaSeconds);

	/** Prints the current camera values so they can be copied into the defaults. */
	void ShowCameraReadout() const;

private:
	void OnAttackPressed();
	void OnAttackReleased();
	void OnSelectSword();
	void OnSelectGun();
	void OnZoomIn();
	void OnZoomOut();
	void OnToggleTuning();
	void OnAimPressed();
	void OnAimReleased();
	void OnCycleCameraMode();

	/** Applies the per-mode input setup (cursor visibility) when the mode changes. */
	void ApplyCameraMode();

	EMsCameraMode LastAppliedMode = EMsCameraMode::Fixed;
	bool bModeApplied = false;

	/** Yaw and pitch captured when aiming began, for the restore flags. */
	float YawBeforeAiming = 0.0f;
	float PitchBeforeAiming = 0.0f;

	/** Previous frame's control rotation, so orbit can work in deltas and scale them. */
	float LastControlYaw = 0.0f;
	float LastControlPitch = 0.0f;

	/** Seeds the delta tracking when orbit starts, so the first frame does not jump. */
	void SeedOrbitTracking();

	/** Right mouse held. Zoom only actually engages when the gun is out. */
	bool bAimHeld = false;

	/** Eased toward the aim targets so the zoom is smooth rather than instant. */
	float CurrentFOV = 0.0f;
	float CurrentDistance = 0.0f;

	/** Smoothed camera lean, kept between frames so it eases rather than snaps. */
	FVector CurrentPeekOffset = FVector::ZeroVector;

	/** 0..1. Decays every frame; shake magnitude is trauma squared. */
	float ShakeTrauma = 0.0f;

	/** Free-running clock driving the sway and the shake noise. */
	float SwayTime = 0.0f;

	/** On-screen readout of the current weapon. Debug only. */
	void ShowWeaponFeedback() const;

	/**
	 * Found on the Blueprint rather than created here, so the template's rig stays intact.
	 *
	 * Names must NOT be CameraBoom / FollowCamera - the Third Person template Blueprint
	 * already has components with those names, and a child Blueprint cannot declare a
	 * variable that collides with one in its C++ parent. It fails to compile if they clash.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedCameraBoom;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedFollowCamera;
};
