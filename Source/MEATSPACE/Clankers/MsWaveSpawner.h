#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsWaveSpawner.generated.h"

class AMsClankerBase;

/** One clanker type in the spawn table, with when and how often it appears. */
USTRUCT(BlueprintType)
struct FMsSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AMsClankerBase> ClankerClass;

	/** Relative chance against the other eligible entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Not spawned before this wave. Use it to introduce types gradually. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 FirstWave = 1;

	/**
	 * Ceiling on how many of THIS type may be alive at once. 0 means unlimited.
	 *
	 * Weight controls how often a type appears; this controls how many can pile up. They are
	 * different problems: a type the player tends to ignore or struggles to kill accumulates
	 * across waves no matter how rare each individual spawn is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MaxAlive = 0;
};

/**
 * Spawns escalating waves of clankers in a ring around the player.
 *
 * This is the game loop. Everything else - districts, bosses, classes - is built on top of
 * "survive what is coming", so it needs to exist before more enemy types do: each new clanker
 * becomes instantly testable at scale instead of being dragged into a level by hand.
 *
 * Spawns happen in a ring far enough out to be off-camera, and the ring is a radius rather
 * than fixed spawn points so it works in any size of arena without re-authoring.
 *
 * Server-authoritative: clankers exist on the server and replicate down.
 */
UCLASS()
class MEATSPACE_API AMsWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMsWaveSpawner();

	virtual void Tick(float DeltaSeconds) override;

	/** Starts, or restarts from wave 1. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Waves")
	void StartWaves();

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Waves")
	void StopWaves();

	/** Skips the countdown and sends the next wave immediately. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Waves")
	void ForceNextWave();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Waves")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Waves")
	int32 GetAliveCount() const { return AliveClankers.Num(); }

protected:
	virtual void BeginPlay() override;

	/** What can spawn. Defaults to the small clanker and the flyer, so it works out of the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves")
	TArray<FMsSpawnEntry> SpawnTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves")
	bool bAutoStart = true;

	/** Seconds before the first wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves", meta = (ClampMin = "0.0"))
	float FirstWaveDelay = 3.0f;

	/** Seconds between waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves", meta = (ClampMin = "1.0"))
	float TimeBetweenWaves = 14.0f;

	/** Clankers in wave 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves", meta = (ClampMin = "1"))
	int32 BaseWaveCount = 5;

	/** Extra clankers added per wave. Fractional, so growth can be gentle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves", meta = (ClampMin = "0.0"))
	float WaveCountGrowth = 2.5f;

	/**
	 * Hard ceiling on live clankers. Protects the framerate, and stops a death spiral where
	 * dying to a wave leaves the next one arriving on top of the survivors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves", meta = (ClampMin = "1"))
	int32 MaxAliveClankers = 60;

	/** Inner edge of the spawn ring. Should be beyond what the camera shows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Placement", meta = (ClampMin = "100.0"))
	float MinSpawnRadius = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Placement", meta = (ClampMin = "100.0"))
	float MaxSpawnRadius = 4200.0f;

	/** How far down to look for floor beneath a candidate spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Placement", meta = (ClampMin = "100.0"))
	float GroundTraceDistance = 3000.0f;

	/** Height above the found floor to spawn at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Placement", meta = (ClampMin = "0.0"))
	float SpawnHeightOffset = 60.0f;

	/** Attempts to find valid ground per clanker before giving up on that one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Placement", meta = (ClampMin = "1"))
	int32 PlacementAttempts = 8;

	/** On-screen wave/alive/countdown readout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Waves|Debug")
	bool bShowDebugReadout = true;

	void SpawnWave();
	void SpawnOne(TSubclassOf<AMsClankerBase> ClankerClass, const FVector& Around);
	bool FindSpawnLocation(const FVector& Around, FVector& OutLocation) const;
	TSubclassOf<AMsClankerBase> PickClankerClass() const;

	/** How many live clankers of exactly this class the spawner is tracking. */
	int32 CountAliveOfClass(TSubclassOf<AMsClankerBase> ClankerClass) const;

	/** Drops dead or destroyed clankers from the live list. */
	void PruneAliveList();

	APawn* FindPlayer() const;

private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AMsClankerBase>> AliveClankers;

	int32 CurrentWave = 0;
	float TimeUntilNextWave = 0.0f;
	bool bRunning = false;
};
