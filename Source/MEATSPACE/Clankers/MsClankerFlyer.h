#pragma once

#include "CoreMinimal.h"
#include "Clankers/MsClankerBase.h"
#include "MsClankerFlyer.generated.h"

/**
 * Flying clanker. Deliberately hard to track.
 *
 * Its movement is three things fighting each other:
 *   - a radial term that holds it at PreferredDistance, so it never commits to closing
 *   - a lateral strafe on a sine wave, so it is always sliding sideways
 *   - a wander bias that re-rolls at random intervals, so the sine never becomes a rhythm
 *     you can lead by instinct
 *
 * That last part is the important one. Pure sine strafing looks erratic for about five
 * seconds and then becomes trivially predictable. Re-rolling the bias at irregular intervals
 * is what keeps it feeling alive - and what makes hitscan the right answer to it, since you
 * cannot reliably lead a target that changes its mind.
 */
UCLASS()
class MEATSPACE_API AMsClankerFlyer : public AMsClankerBase
{
	GENERATED_BODY()

public:
	AMsClankerFlyer();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual FVector ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer) override;

	// --- Weapon ---
	//
	// Telegraphed hitscan rather than an instant shot. The wind-up is the whole point: it
	// makes the attack dodgeable by moving, which is what turns flyers from an annoyance into
	// something you must actually respond to. An instant unavoidable shot would just be a
	// health tax.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon")
	bool bCanShoot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon", meta = (ClampMin = "0.0"))
	float ShotDamage = 9.0f;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon", meta = (ClampMin = "0.1"))
	float FireInterval = 3.2f;

	/** Wind-up before firing. Your window to break line of sight or move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon", meta = (ClampMin = "0.05"))
	float TelegraphTime = 0.9f;

	/** Maximum engagement distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon", meta = (ClampMin = "100.0"))
	float FireRange = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Weapon")
	bool bDrawDebugShot = true;

	/** Fires the shot at the end of the telegraph. Server only. */
	void FireAtPlayer(APawn* TargetPlayer);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFX(const FVector_NetQuantize& From, const FVector_NetQuantize& To, bool bHit);

	/** How high above the player it tries to sit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer")
	float HoverHeight = 320.0f;

	/** Horizontal standoff distance. It closes to this and then circles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "50.0"))
	float PreferredDistance = 520.0f;

	/** How hard it corrects its distance. Raise for a tighter leash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float RadialWeight = 1.0f;

	/**
	 * How hard it slides sideways. Kept moderate: a flyer that is genuinely hard to hit stops
	 * being a target and becomes a chore, especially once it can shoot back and you actually
	 * need to kill it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float StrafeWeight = 0.65f;

	/** Speed of the sideways oscillation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float StrafeFrequency = 1.6f;

	/** How strongly it holds its hover height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float VerticalWeight = 0.9f;

	/** Height of the idle bob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float BobAmplitude = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float BobFrequency = 1.1f;

	/** Shortest gap between direction re-rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.05"))
	float MinWanderInterval = 1.0f;

	/** Longest gap between direction re-rolls. Wider spread = less predictable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.05"))
	float MaxWanderInterval = 2.6f;

	/** How far a re-roll can throw it sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.0"))
	float WanderStrength = 0.45f;

	/** How far a re-roll can throw its hover height, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.0"))
	float VerticalWanderRange = 90.0f;

private:
	void RerollWander();

	/** Random sideways push, re-rolled at irregular intervals. */
	float WanderBias = 0.0f;

	/** Random height offset, re-rolled with the same cadence. */
	float VerticalBias = 0.0f;

	float NextWanderTime = 0.0f;

	/** Per-instance phase so several flyers never oscillate in lockstep. */
	float PhaseOffset = 0.0f;

	float NextFireTime = 0.0f;
	float TelegraphEndTime = 0.0f;
	bool bTelegraphing = false;
};
