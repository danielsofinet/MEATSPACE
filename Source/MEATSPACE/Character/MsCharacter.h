#pragma once

#include "CoreMinimal.h"
#include "Combat/MsCombatTypes.h"
#include "GameFramework/Character.h"
#include "MsCharacter.generated.h"

class UMsWeaponComponent;
class UMsMeleeComponent;
class UMsHealthComponent;
class USpringArmComponent;
class UCameraComponent;

/**
 * Base player character for MEATSPACE.
 *
 * The camera is a forced-perspective third-person rig: the mouse turns it, pitch is clamped
 * to a band that preserves the look, and a narrow FOV compresses depth so the scene reads as
 * near-isometric while big skies still feel dramatic.
 *
 * Aim is a fixed on-screen reticle rather than the mouse cursor, and the aim ray is
 * deprojected through that exact reticle position - so the crosshair can never lie about
 * where shots land. Its position is separately tunable for hipfire and for aiming, because a
 * dead-centre reticle is wrong here: the boom points at the character, so screen centre is
 * the ground just behind him.
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
	UMsHealthComponent* GetHealth() const { return HealthComponent; }

	/** 0..1, fades after taking damage. Drives the HUD's hit flash. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	float GetDamageFlash() const { return DamageFlashAlpha; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	EMsWeaponSlot GetActiveSlot() const { return ActiveSlot; }

	/** Swap weapons. Safe to call on client - replicates to the server. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Combat")
	void EquipSlot(EMsWeaponSlot NewSlot);

	/** World point under the reticle, found by tracing the deprojected reticle ray. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Aim")
	bool ComputeAimPoint(FVector& OutAimPoint) const;

	/** True while right mouse is held with the gun equipped. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	bool IsAiming() const;

	/** Spread multiplier the weapon should apply right now. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	float GetAimSpreadMultiplier() const { return IsAiming() ? AimSpreadMultiplier : 1.0f; }

	/**
	 * Reticle position actually in force this frame, as a fraction of half the screen.
	 * X right, Y down.
	 *
	 * This is the SMOOTHED value, not the raw hip/aim setting. Both the HUD and the aim trace
	 * read it, so the crosshair and the shot stay in lockstep even mid-transition.
	 */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Aim")
	FVector2D GetCrosshairScreenOffset() const { return CurrentCrosshairOffset; }

	/** Adds decaying trauma to the camera. Repeated hits stack into a bigger jolt. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Camera")
	void AddCameraShake(float Trauma);

	/** Called by the weapon and sword so the camera reacts to what you are doing. */
	void OnWeaponFired();
	void OnWeaponHit();
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsHealthComponent> HealthComponent;

	/** Seconds spent dead before respawning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Combat", meta = (ClampMin = "0.1"))
	float RespawnDelay = 2.5f;

	/** Camera trauma when hit. Getting hurt should be felt, not just seen on a bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HurtShake = 0.45f;

	/** How long the red hit flash takes to fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Combat", meta = (ClampMin = "0.05"))
	float DamageFlashDuration = 0.45f;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* DeadActor);

	/** Server-side. Puts the character back at a player start with full health. */
	void Respawn();

	/** Which weapon is out. Replicated so other players can see it later. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlot, BlueprintReadOnly, Category = "Meatspace|Combat")
	EMsWeaponSlot ActiveSlot = EMsWeaponSlot::Gun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Combat")
	EMsWeaponSlot StartingSlot = EMsWeaponSlot::Gun;

	// --- Camera rig ---

	/** Take over the Blueprint's camera boom and drive it from here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	bool bDriveCameraRig = true;

	/** Downward tilt in degrees. Mouse-driven at runtime; this is the starting value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "-60.0", ClampMax = "89.0"))
	float CameraPitch = 18.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	float CameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "100.0"))
	float CameraDistance = 2450.0f;

	/** Narrow FOV compresses depth - this is what creates the forced-perspective look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float CameraFOV = 62.9f;

	/**
	 * Keeps the camera this far above the character's origin, no matter the pitch.
	 *
	 * Looking up swings the boom DOWN behind the character - at 2450 units of arm and -35
	 * degrees of pitch that puts the camera ~1400 units underground. Rather than clamp how
	 * far you can look up (which would make flying clankers unhittable), the boom's pivot is
	 * raised by however much is needed to keep the camera above the floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "0.0"))
	float MinCameraHeight = 160.0f;

	/** How lazily the boom follows the character's position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MinCameraDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "100.0"))
	float MaxCameraDistance = 4210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Zoom", meta = (ClampMin = "10.0"))
	float ZoomStep = 160.0f;

	// --- Mouse look ---

	/**
	 * How far the camera can tilt. NEGATIVE minimum lets it look UP into the sky, which is
	 * essential for flying clankers - a floor of 0 would make anything overhead unhittable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "-80.0", ClampMax = "89.0"))
	float PitchMin = -35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "-80.0", ClampMax = "89.0"))
	float PitchMax = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.05"))
	float YawSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.05"))
	float AimYawSensitivity = 1.15f;

	/** Vertical is more sensitive than horizontal at the same raw input, so scale it down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.05"))
	float PitchSensitivity = 0.7f;

	/**
	 * How sluggishly the camera catches up to the mouse. 0 is instant and rigid, 1 is heavy
	 * and floaty. Some dullness gives the camera weight; too much feels like lag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CameraDullness = 0.86f;

	/** Dullness while aiming. Lower than hipfire - precision wants a rigid camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimCameraDullness = 0.30f;

	/** How fast the character's body swings round to face where the camera looks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Look", meta = (ClampMin = "0.1"))
	float CharacterTurnSpeed = 14.0f;

	// --- Reticle ---

	/**
	 * Reticle position while NOT aiming, as a fraction of half the screen. Y is negative for
	 * up. Pushed well above centre so it does not sit on the character's head.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Reticle")
	FVector2D HipCrosshairOffset = FVector2D(0.076f, -0.142f);

	/** Reticle position while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Reticle")
	FVector2D AimCrosshairOffset = FVector2D(-0.040f, -0.120f);

	/** Camera shoulder shift while NOT aiming. X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Reticle")
	FVector HipSocketOffset = FVector(0.0f, 0.0f, 0.0f);

	/** Camera shoulder shift while aiming - moves the character out from under the reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Reticle")
	FVector AimSocketOffset = FVector(0.0f, 110.0f, 55.0f);

	// --- Aim-down-sights ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom")
	bool bAllowAimZoom = true;

	/**
	 * 0.28 of a 62.9 base gives ~17.6 degrees while aiming - a strong magnification, which is
	 * what the tuning session converged on. The base FOV stays wide for hipfire.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimFOVMultiplier = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimDistanceMultiplier = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.5"))
	float AimTransitionSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Aim|Zoom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSpreadMultiplier = 0.25f;

	// --- Camera life ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice")
	bool bCameraSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayAmplitude = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayFrequency = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float SwayMoveBoost = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float ShakeAngle = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0"))
	float ShakeRoll = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.1"))
	float ShakeFrequency = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.1"))
	float ShakeDecayRate = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireShake = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GunHitShake = 0.10f;

	/** Zero by design: swinging through air should not move the camera, or a whiff reads as a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordSwingShake = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Juice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordHitShake = 0.38f;

	// --- Live tuning (development tool) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	bool bCameraTuningMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera|Tuning")
	float FOVAdjustRate = 14.0f;

	UFUNCTION()
	void OnRep_ActiveSlot();

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(EMsWeaponSlot NewSlot);

	void ApplyCameraSettings();
	void TickMouseLook(float DeltaSeconds);
	void TickCameraTuning(float DeltaSeconds);
	void TickCameraJuice(float DeltaSeconds);
	void ShowCameraReadout() const;

	/** Reticle offset currently in force, chosen by whether we are aiming. */
	FVector2D& ActiveCrosshairOffset() { return IsAiming() ? AimCrosshairOffset : HipCrosshairOffset; }
	FVector& ActiveSocketOffset() { return IsAiming() ? AimSocketOffset : HipSocketOffset; }

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

	void ShowWeaponFeedback() const;

	/** Seeds mouse-look tracking so the first frame never jumps. */
	void SeedLookTracking();

	/** Blueprint's boom and camera. Names must not collide with the Blueprint's own. */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedCameraBoom;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedFollowCamera;

	bool bAimHeld = false;
	bool bInputConfigured = false;

	/** Where the mouse says the camera should be, before dullness smoothing. */
	float DesiredCameraYaw = 0.0f;
	float DesiredCameraPitch = 24.0f;

	float LastControlYaw = 0.0f;
	float LastControlPitch = 0.0f;

	float CurrentFOV = 0.0f;
	float CurrentDistance = 0.0f;

	/** Eased reticle position, so it travels with the zoom instead of teleporting. */
	FVector2D CurrentCrosshairOffset = FVector2D::ZeroVector;
	bool bCrosshairInitialised = false;

	float ShakeTrauma = 0.0f;
	float SwayTime = 0.0f;

	/** Fades from 1 to 0 after taking damage. */
	float DamageFlashAlpha = 0.0f;

	bool bIsDead = false;
	FTimerHandle RespawnTimer;
};
