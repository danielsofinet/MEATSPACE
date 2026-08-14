#pragma once

#include "CoreMinimal.h"
#include "Clankers/MsSpawnTypes.h"
#include "GameFramework/Actor.h"
#include "MsEncounterVolume.generated.h"

class AMsClankerBase;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnEncounterTriggered, class AMsEncounterVolume*, Encounter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnEncounterCleared, class AMsEncounterVolume*, Encounter);

/**
 * A fight that belongs to a place.
 *
 * Walk into the box and clankers spawn around the volume. Kill them all and the encounter
 * reports CLEARED. That signal is the point of this class: "cleared" is the atom of district
 * progression, so gates, objectives and eventually boss unlocks all hang off it.
 *
 * This replaces the wave spawner as the game's actual pacing mechanism - MEATSPACE is not
 * arena survival, it is advancing through a place and meeting what is there. The wave spawner
 * survives as a stress-testing tool.
 *
 * Reinforcements arrive when the fight thins out rather than on a timer, so the encounter
 * paces itself to how fast the player actually kills things.
 */
UCLASS()
class MEATSPACE_API AMsEncounterVolume : public AActor
{
	GENERATED_BODY()

public:
	AMsEncounterVolume();

	virtual void Tick(float DeltaSeconds) override;

	/** Starts the encounter regardless of whether the player is inside. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Encounter")
	void TriggerEncounter();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Encounter")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Encounter")
	bool IsCleared() const { return bCleared; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Encounter")
	int32 GetAliveCount() const;

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Encounter")
	FMsOnEncounterTriggered OnEncounterTriggered;

	/** Fires once every clanker this encounter spawned is dead. */
	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Encounter")
	FMsOnEncounterCleared OnEncounterCleared;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	/** The box the player has to walk into. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Encounter")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter")
	TArray<FMsSpawnEntry> SpawnTable;

	/** Total clankers across the whole encounter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter", meta = (ClampMin = "1"))
	int32 TotalToSpawn = 8;

	/**
	 * How many arrive at once. The rest are held back as reinforcements, which is what stops
	 * an encounter being one overwhelming lump followed by nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter", meta = (ClampMin = "1"))
	int32 SpawnPerRound = 4;

	/** Next round arrives once the live count drops to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter", meta = (ClampMin = "0"))
	int32 ReinforceWhenAliveAtOrBelow = 1;

	/** Fires only once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter")
	bool bTriggerOnce = true;

	// --- Placement ---

	/** Clankers appear in a ring this far from the volume's centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "0.0"))
	float MinSpawnRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "1.0"))
	float MaxSpawnRadius = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "100.0"))
	float GroundTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "0.0"))
	float SpawnHeightOffset = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "1"))
	int32 PlacementAttempts = 10;

	// --- Story hooks, so an encounter can be wired without Blueprint script ---

	/** Objective set the moment the fight starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Story")
	FText ObjectiveOnTriggered;

	/** Objective set once it is cleared. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Story")
	FText ObjectiveOnCleared;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Meatspace|Encounter|Story")
	TObjectPtr<AActor> ObjectiveTargetOnCleared;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Debug")
	bool bShowDebugReadout = false;

	void SpawnRound();
	void SpawnOne(TSubclassOf<AMsClankerBase> ClankerClass);
	bool FindSpawnLocation(FVector& OutLocation) const;
	TSubclassOf<AMsClankerBase> PickClankerClass() const;
	int32 CountAliveOfClass(TSubclassOf<AMsClankerBase> ClankerClass) const;
	void PruneAliveList();
	void MarkCleared();

private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AMsClankerBase>> AliveClankers;

	bool bActive = false;
	bool bCleared = false;
	int32 SpawnedSoFar = 0;
	int32 RoundIndex = 0;
};
