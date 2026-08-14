#pragma once

#include "CoreMinimal.h"
#include "Clankers/MsSpawnTypes.h"
#include "GameFramework/Actor.h"
#include "MsEncounterVolume.generated.h"

class AMsClankerBase;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnEncounterTriggered, class AMsEncounterVolume*, Encounter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnEncounterCleared, class AMsEncounterVolume*, Encounter);

/** How an encounter decides to send the next group. */
UENUM(BlueprintType)
enum class EMsEncounterPacing : uint8
{
	/**
	 * Reinforce when the fight thins out, and end only when everything is dead.
	 * Paces itself to the player's skill - kill faster, get reinforced sooner.
	 */
	ClearToProceed		UMETA(DisplayName = "Clear to proceed"),

	/**
	 * Escalate on a clock and end after a fixed time, whatever the player is doing.
	 * Indifferent to how well you are doing, which is exactly what makes it feel like
	 * losing control - the reinforcements arrive because time passed, not because you
	 * earned them.
	 */
	TimedEscalation		UMETA(DisplayName = "Timed escalation")
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter")
	EMsEncounterPacing Pacing = EMsEncounterPacing::TimedEscalation;

	// --- Timed escalation ---

	/**
	 * How many arrive in each round, in order. The default 1, 2, 3, 5 is the onboarding
	 * ambush: one clanker takes exception to your sword, then it calls friends, and it gets
	 * away from you fast.
	 *
	 * An explicit list rather than a growth formula, because the shape of an escalation is a
	 * design decision and you should be able to see it at a glance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Timed")
	TArray<int32> RoundCounts;

	/** Seconds between rounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Timed", meta = (ClampMin = "0.5"))
	float RoundInterval = 6.0f;

	/**
	 * Encounter ends after this many seconds regardless of what is still alive.
	 * 0 means it runs until every round is spawned and dead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Timed", meta = (ClampMin = "0.0"))
	float MaxDuration = 30.0f;

	/**
	 * Remove whatever is still alive when the encounter ends.
	 *
	 * Without this, a timed encounter that ends while you are surrounded just leaves you
	 * surrounded, and "it is over, move on" becomes a lie.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Timed")
	bool bDespawnRemainingOnEnd = true;

	// --- Clear to proceed ---

	/** Total clankers across the whole encounter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Clear", meta = (ClampMin = "1"))
	int32 TotalToSpawn = 8;

	/**
	 * How many arrive at once. The rest are held back as reinforcements, which is what stops
	 * an encounter being one overwhelming lump followed by nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Clear", meta = (ClampMin = "1"))
	int32 SpawnPerRound = 4;

	/** Next round arrives once the live count drops to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Clear", meta = (ClampMin = "0"))
	int32 ReinforceWhenAliveAtOrBelow = 1;

	/** Fires only once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter")
	bool bTriggerOnce = true;

	// --- Delivery ---

	/**
	 * Clankers arrive by dropship instead of appearing on the ground.
	 *
	 * Worth it for more than fiction: a pod falling for a second telegraphs exactly where the
	 * next group lands, which turns escalation into something the player can respond to
	 * rather than something that simply happens around them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Delivery")
	bool bDeliverByDropPod = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Delivery")
	TSubclassOf<class AMsDropPod> DropPodClass;

	/** Clankers per pod. More than this in a round means more than one pod. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Delivery", meta = (ClampMin = "1"))
	int32 PodCapacity = 6;

	/** Tracks clankers that arrived by pod, so the encounter still knows when it is clear. */
	UFUNCTION()
	void HandleClankerDelivered(AMsClankerBase* Clanker);

	// --- Placement ---

	/**
	 * Distance from the volume that clankers arrive, in cm. 1000-1500 is 10-15 metres.
	 *
	 * Combined with the arc below, this is what stops dropships landing behind the player,
	 * where they could be run past without ever being seen. A telegraph nobody sees is not a
	 * telegraph.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "0.0"))
	float MinSpawnRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "1.0"))
	float MaxSpawnRadius = 1500.0f;

	/**
	 * Width of the arc that spawns are confined to, centred on the volume's forward direction
	 * (the arrow, visible when it is selected). 360 spawns all the way around.
	 *
	 * Rotate the volume in the level so its arrow points where you want them to arrive -
	 * normally ahead of the player, blocking the way on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float SpawnArcDegrees = 150.0f;

	/** Rotates the arc off the forward direction. 180 puts spawns behind the volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Encounter|Placement", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float SpawnArcOffsetDegrees = 0.0f;

	/** Editor-only arrow showing which way the volume faces. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Encounter")
	TObjectPtr<class UArrowComponent> FacingArrow;

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

	/** Spawns a specific number, for timed rounds. */
	void SpawnCount(int32 Count);

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

	/** Seconds since the encounter started, for timed pacing. */
	float ElapsedTime = 0.0f;
	float NextRoundTime = 0.0f;

	/**
	 * Clankers dispatched but not yet unloaded. Without this the encounter sees zero alive
	 * while pods are still falling and declares itself cleared mid-drop.
	 */
	int32 PendingDeliveries = 0;
};
