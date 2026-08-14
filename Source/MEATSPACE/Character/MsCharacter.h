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

private:
	void OnAttackPressed();
	void OnAttackReleased();
	void OnSelectSword();
	void OnSelectGun();
	void OnZoomIn();
	void OnZoomOut();

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
