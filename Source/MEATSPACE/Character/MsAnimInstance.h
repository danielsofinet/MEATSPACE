#pragma once

#include "Animation/AnimInstance.h"
#include "Combat/MsCombatTypes.h"
#include "CoreMinimal.h"
#include "MsAnimInstance.generated.h"

class AMsCharacter;
class UCharacterMovementComponent;

/**
 * Everything the Animation Blueprint needs, computed in C++.
 *
 * The graph should contain no maths - it reads these values and blends. Keeping the
 * calculation here means it is diffable, reviewable and testable, and the Blueprint stays a
 * thin layer of "which animation plays when", which is the part that genuinely benefits from
 * being visual.
 *
 * The important one is Direction. MEATSPACE's character faces where the camera looks rather
 * than where they walk, so they are constantly strafing and back-pedalling. Without a
 * direction value driving a blendspace, every sideways step plays a forward run and the
 * character reads as sliding rather than moving.
 */
UCLASS()
class MEATSPACE_API UMsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// --- Locomotion ---

	/** Horizontal speed in cm/s. Drives the walk/run blend. */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	float GroundSpeed = 0.0f;

	/**
	 * Movement direction relative to where the character faces, -180 to 180.
	 * 0 = forward, 90 = right, -90 = left, 180 = backwards.
	 *
	 * This is the axis of a directional blendspace, and the whole reason strafing looks right.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	float Direction = 0.0f;

	/**
	 * True when the character is actually trying to move.
	 *
	 * Tested against acceleration rather than speed alone, so the idle plays the instant input
	 * stops instead of waiting for momentum to bleed off - which is what makes stopping feel
	 * responsive rather than mushy.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	bool bIsInAir = false;

	/** Vertical velocity. Lets a fall blend by how fast it is falling. */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	float VerticalVelocity = 0.0f;

	/**
	 * Degrees per second of turn, smoothed. Drives lean into turns.
	 * Positive is turning right.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Locomotion")
	float TurnRate = 0.0f;

	// --- Aiming ---
	//
	// Fed into an aim offset so the upper body points where the player is aiming, rather than
	// the character shooting straight ahead while the reticle is somewhere else.

	/** Up/down aim relative to the character's facing, in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Aim")
	float AimPitch = 0.0f;

	/** Left/right aim relative to the character's facing, in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Aim")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Aim")
	bool bIsAiming = false;

	// --- Combat state ---

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Combat")
	EMsWeaponSlot ActiveSlot = EMsWeaponSlot::Gun;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Combat")
	bool bHasWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Combat")
	bool bIsSwinging = false;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Combat")
	bool bIsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "Meatspace|Combat")
	bool bIsDead = false;

	/** How fast TurnRate catches up. Lower is smoother and lazier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Meatspace|Locomotion", meta = (ClampMin = "0.1"))
	float TurnRateSmoothing = 6.0f;

	/** Speed below which the character counts as stopped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Meatspace|Locomotion", meta = (ClampMin = "0.0"))
	float MoveThreshold = 3.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMsCharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;

	/** Last frame's yaw, for computing turn rate. */
	float PreviousYaw = 0.0f;
};
