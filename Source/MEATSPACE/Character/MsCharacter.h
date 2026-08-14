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
	void OnSwordSwing();
	void OnSwordHit();

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

	/** Downward tilt in degrees. 90 would be straight down; 50-60 reads as forced perspective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float CameraPitch = 52.0f;

	/** World yaw the camera looks along. Rotate the whole view by changing this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	float CameraYaw = 0.0f;

	/** Boom length. Bigger = further out = more battlefield visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "100.0"))
	float CameraDistance = 1650.0f;

	/**
	 * Narrow FOV is what creates the forced-perspective look. 90 is a normal game camera;
	 * 30-45 flattens the scene toward isometric while keeping perspective depth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float CameraFOV = 38.0f;

	/** How lazily the camera follows. Lower = floatier, higher = glued to the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 9.0f;

	/** Scroll wheel zoom limits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MinCameraDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MaxCameraDistance = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "10.0"))
	float ZoomStep = 160.0f;

	// --- Mouse-follow. The camera leans toward the cursor instead of staying centred. ---

	/**
	 * How far the camera drifts toward the cursor, as a fraction of the distance to it.
	 * 0 = camera stays locked on the character. 1 = camera sits on the cursor.
	 * Around 0.3-0.4 is the usual sweet spot: you see further in the direction you are
	 * aiming without losing sight of your own character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MousePeekStrength = 0.35f;

	/** Hard cap on the lean, in cm, so the character never leaves the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.0"))
	float MaxPeekDistance = 700.0f;

	/** How lazily the lean follows the cursor. Lower = smoother, higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "0.1"))
	float PeekLagSpeed = 5.0f;

	/** Let Q and E swing the camera yaw around the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse")
	bool bAllowCameraRotate = true;

	/** Degrees per second while Q or E is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Mouse", meta = (ClampMin = "1.0"))
	float CameraRotateRate = 90.0f;

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

	/** Trauma added per gunshot. Small - it fires eight times a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireShake = 0.14f;

	/** Trauma added when a swing starts. The weight of the swing itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordSwingShake = 0.20f;

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
