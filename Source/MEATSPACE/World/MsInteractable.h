#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsInteractable.generated.h"

class AMsCharacter;
class USceneComponent;

/**
 * Anything the player can walk up to and press E on.
 *
 * Keeps a static registry of live interactables so the character can find the nearest one
 * without a world query every frame. There will only ever be a handful per level, so a linear
 * scan is far cheaper than an overlap sweep.
 */
UCLASS(Abstract)
class MEATSPACE_API AMsInteractable : public AActor
{
	GENERATED_BODY()

public:
	AMsInteractable();

	/** Called when the player presses the interact key while this is the nearest target. */
	virtual void Interact(AMsCharacter* Player);

	/** Text shown in the interaction prompt. FText because a player reads it. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Interaction")
	virtual FText GetPrompt() const { return Prompt; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Interaction")
	bool CanInteract() const { return bInteractable; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Interaction")
	float GetInteractRadius() const { return InteractRadius; }

	/** Every live interactable, for the character's nearest-target search. */
	static const TArray<TWeakObjectPtr<AMsInteractable>>& GetAllInteractables();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Interaction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Interaction")
	FText Prompt;

	/** How close the player must be. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Interaction", meta = (ClampMin = "10.0"))
	float InteractRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Interaction")
	bool bInteractable = true;

private:
	static TArray<TWeakObjectPtr<AMsInteractable>> AllInteractables;
};
