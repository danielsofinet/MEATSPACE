#pragma once

#include "CoreMinimal.h"
#include "Clankers/MsClankerBase.h"
#include "MsClankerSmall.generated.h"

/**
 * Small ground clanker. Moves as a hive.
 *
 * Steering is classic boids layered on top of "chase the player":
 *   - Separation keeps them from stacking into one cube
 *   - Cohesion pulls stragglers back toward the group
 *   - Alignment makes them turn together, which is what actually reads as a swarm
 *   - Seek drives the whole mass at the player
 *
 * The result is a group that flows around obstacles and arrives as a wave rather than a
 * single-file queue. Tuning the four weights changes the personality completely - heavy
 * separation gives a loose scatter, heavy alignment gives a tight shoal.
 */
UCLASS()
class MEATSPACE_API AMsClankerSmall : public AMsClankerBase
{
	GENERATED_BODY()

public:
	AMsClankerSmall();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FVector ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer) override;

	/** Pull toward the player. The reason the swarm is a threat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "0.0"))
	float SeekWeight = 1.0f;

	/** Push away from crowded neighbours. Raise it if they clump into one blob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "0.0"))
	float SeparationWeight = 1.8f;

	/** Pull toward the group's centre. Raise it to keep the hive tight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "0.0"))
	float CohesionWeight = 0.55f;

	/** Match the group's heading. This is what makes them turn as one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 0.8f;

	/** Who counts as part of my group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "1.0"))
	float NeighbourRadius = 550.0f;

	/** Personal space. Inside this, separation kicks in hard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Hive", meta = (ClampMin = "1.0"))
	float SeparationRadius = 160.0f;

private:
	/**
	 * Every live small clanker, so each one can find its neighbours without a world query
	 * per frame. Fine at prototype scale; becomes a spatial hash if we ever push hundreds.
	 */
	static TArray<TWeakObjectPtr<AMsClankerSmall>> LiveClankers;
};
