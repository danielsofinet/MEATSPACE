#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MsObjectiveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMsOnObjectiveChanged);

/**
 * The one current objective: what the player is meant to be doing, and optionally where.
 *
 * A world subsystem rather than an actor, so nothing has to be placed in the level and
 * anything can reach it - NPCs, triggers, the HUD - without wiring references between them.
 *
 * Deliberately single-objective. A quest log is a real feature and the onboarding does not
 * need one; it needs "go and see the neighbour" on screen with an arrow.
 */
UCLASS()
class MEATSPACE_API UMsObjectiveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Sets the current objective. Target may be null for objectives with no location. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Objectives")
	void SetObjective(const FText& Text, AActor* Target = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Objectives")
	void ClearObjective();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Objectives")
	FText GetObjectiveText() const { return ObjectiveText; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Objectives")
	AActor* GetObjectiveTarget() const { return ObjectiveTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Objectives")
	bool HasObjective() const { return !ObjectiveText.IsEmpty(); }

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Objectives")
	FMsOnObjectiveChanged OnObjectiveChanged;

private:
	UPROPERTY()
	FText ObjectiveText;

	UPROPERTY()
	TWeakObjectPtr<AActor> ObjectiveTarget;
};
