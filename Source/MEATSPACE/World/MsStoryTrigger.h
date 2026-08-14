#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsStoryTrigger.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnStoryTriggered, class AMsStoryTrigger*, Trigger);

/**
 * A box that makes something happen when the player walks into it.
 *
 * Set an objective, play a cutscene, or just broadcast so something else can react. This is
 * the glue that turns a blockout into a level: arriving at the neighbour's door is a trigger,
 * so is stepping out of grandpa's garden.
 *
 * Everything is optional, so one class covers "set the next objective", "start the cutscene"
 * and "notify a door to open" without three separate actors to learn.
 */
UCLASS()
class MEATSPACE_API AMsStoryTrigger : public AActor
{
	GENERATED_BODY()

public:
	AMsStoryTrigger();

	/** Fires the trigger without needing the player to walk in. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Story")
	void Fire();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Story")
	bool HasFired() const { return bFired; }

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Story")
	FMsOnStoryTriggered OnStoryTriggered;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Story")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story")
	bool bTriggerOnce = true;

	/** Objective set when this fires. Leave empty to change nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story")
	FText ObjectiveOnTrigger;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Meatspace|Story")
	TObjectPtr<AActor> ObjectiveTarget;

	/** Cutscene lines. Leave empty for no cutscene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Cutscene", meta = (MultiLine = "true"))
	TArray<FText> CutsceneLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Cutscene", meta = (ClampMin = "0.2"))
	float SecondsPerLine = 3.5f;

	// --- Rewards. Applied AFTER the cutscene finishes, so the upgrade lands on the beat
	// rather than during a fade to black where nobody would see it. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Reward")
	bool bGrantSword = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Reward")
	bool bGrantGun = false;

	/** Shield granted. 0 leaves the shield alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Reward", meta = (ClampMin = "0.0"))
	float GrantShieldAmount = 0.0f;

	/** Objective set once the cutscene has finished and rewards are handed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Story|Reward")
	FText ObjectiveAfterCutscene;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Meatspace|Story|Reward")
	TObjectPtr<AActor> ObjectiveTargetAfterCutscene;

	UFUNCTION()
	void HandleCutsceneFinished();

	/** Hands out whatever this trigger grants. */
	void ApplyRewards();

private:
	bool bFired = false;
};
