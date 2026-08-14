#pragma once

#include "CoreMinimal.h"
#include "MsSpawnTypes.generated.h"

class AMsClankerBase;

/**
 * One clanker type in a spawn table, with when and how often it appears.
 *
 * Shared by the wave spawner (stress testing) and encounter volumes (the actual game), so
 * both get the same weighting, gating and per-type caps without duplicating the concept.
 */
USTRUCT(BlueprintType)
struct FMsSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AMsClankerBase> ClankerClass;

	/** Relative chance against the other eligible entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Not spawned before this wave/round. Use it to introduce types gradually. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 FirstWave = 1;

	/**
	 * Ceiling on how many of THIS type may be alive at once. 0 means unlimited.
	 *
	 * Weight controls how often a type appears; this controls how many can pile up. They are
	 * different problems: a type the player struggles to reach accumulates regardless of how
	 * rare each individual spawn is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MaxAlive = 0;
};
