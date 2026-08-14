#pragma once

#include "CoreMinimal.h"
#include "Combat/MsCombatTypes.h"
#include "World/MsInteractable.h"
#include "MsNpc.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnDialogueFinished, AMsNpc*, Npc);

/**
 * A character you talk to. Grandpa, the neighbour, and whoever else the story needs.
 *
 * Dialogue is a list of FText lines advanced with the interact key. Deliberately simple - a
 * branching dialogue system is a whole feature and the onboarding does not need one; what it
 * needs is for someone to hand you a sword and tell you where to go.
 *
 * The "and then" hooks (grant a weapon, set the next objective) live here rather than in
 * Blueprint script so the onboarding can be wired up entirely by placing actors and filling
 * in fields.
 */
UCLASS()
class MEATSPACE_API AMsNpc : public AMsInteractable
{
	GENERATED_BODY()

public:
	AMsNpc();

	virtual void Interact(AMsCharacter* Player) override;
	virtual FText GetPrompt() const override;

	UFUNCTION(BlueprintPure, Category = "Meatspace|Dialogue")
	bool IsTalking() const { return bTalking; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Dialogue")
	FText GetSpeakerName() const { return SpeakerName; }

	/** The line currently on screen. Empty if not talking. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Dialogue")
	FText GetCurrentLine() const;

	/** Fires after the last line. Hook extra behaviour here from Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Dialogue")
	FMsOnDialogueFinished OnDialogueFinished;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Npc")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue")
	FText SpeakerName;

	/** Said in order, one press of the interact key per line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue", meta = (MultiLine = "true"))
	TArray<FText> Lines;

	/** Lines used on every conversation after the first. Falls back to Lines if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue", meta = (MultiLine = "true"))
	TArray<FText> RepeatLines;

	/** Prompt shown once this NPC has already been talked to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue")
	FText RepeatPrompt;

	// --- What happens when the conversation ends ---

	/** Grandpa's sword. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue|Outcome")
	bool bGrantSword = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue|Outcome")
	bool bGrantGun = false;

	/** Objective set when the conversation ends. Leave empty to change nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue|Outcome")
	FText ObjectiveOnFinish;

	/** Actor the objective points at - the marker arrow tracks it. Pick one in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Meatspace|Dialogue|Outcome")
	TObjectPtr<AActor> ObjectiveTargetOnFinish;

	/** Only talkable once. Useful for a one-shot story beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Dialogue|Outcome")
	bool bSingleUse = false;

	void FinishDialogue(AMsCharacter* Player);

	/** Which line list this conversation is reading from. */
	const TArray<FText>& ActiveLines() const;

private:
	bool bTalking = false;
	bool bHasTalkedBefore = false;
	int32 LineIndex = 0;
};
