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

	virtual FVector ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer) override;

	/** How high above the player it tries to sit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer")
	float HoverHeight = 320.0f;

	/** Horizontal standoff distance. It closes to this and then circles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "50.0"))
	float PreferredDistance = 520.0f;

	/** How hard it corrects its distance. Raise for a tighter leash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float RadialWeight = 1.0f;

	/** How hard it slides sideways. This is the main source of "annoying to hit". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer", meta = (ClampMin = "0.0"))
	float StrafeWeight = 1.3f;

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
	float MinWanderInterval = 0.5f;

	/** Longest gap between direction re-rolls. Wider spread = less predictable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.05"))
	float MaxWanderInterval = 1.6f;

	/** How far a re-roll can throw it sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.0"))
	float WanderStrength = 1.2f;

	/** How far a re-roll can throw its hover height, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Flyer|Erratic", meta = (ClampMin = "0.0"))
	float VerticalWanderRange = 150.0f;

private:
	void RerollWander();

	/** Random sideways push, re-rolled at irregular intervals. */
	float WanderBias = 0.0f;

	/** Random height offset, re-rolled with the same cadence. */
	float VerticalBias = 0.0f;

	float NextWanderTime = 0.0f;

	/** Per-instance phase so several flyers never oscillate in lockstep. */
	float PhaseOffset = 0.0f;
};
