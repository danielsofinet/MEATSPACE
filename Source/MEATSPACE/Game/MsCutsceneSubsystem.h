#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MsCutsceneSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMsOnCutsceneFinished);

/**
 * Placeholder cutscene: fades to black and shows a sequence of lines.
 *
 * Not a real cutscene system - that is Sequencer, cameras and animation, and it should not be
 * built until there are characters and an environment worth filming. What this does provide
 * is the *hook*: the moment in the level where a cutscene belongs, wired up and firing, so
 * the level's structure can be finished and tested now and the real thing dropped in later
 * without moving anything.
 *
 * Player input is disabled for the duration, which is most of what a cutscene does to the
 * game anyway.
 */
UCLASS()
class MEATSPACE_API UMsCutsceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Starts a cutscene. Ignored if one is already playing. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Cutscene")
	void PlayCutscene(const TArray<FText>& Lines, float SecondsPerLine = 3.5f, bool bClearBattlefield = true);

	/**
	 * Stops every running encounter and removes every clanker in the world.
	 *
	 * A cutscene has to clear the board. Fading back in to a pack that was quietly waiting
	 * during the story beat breaks the fiction and, worse, kills the player during a moment
	 * they had no reason to think was dangerous.
	 */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Cutscene")
	void ClearBattlefield();

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Cutscene")
	void SkipCutscene();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Cutscene")
	bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Cutscene")
	FText GetCurrentLine() const;

	/** 0..1 black overlay opacity, including the fade in and out. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Cutscene")
	float GetOverlayAlpha() const;

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Cutscene")
	FMsOnCutsceneFinished OnCutsceneFinished;

	/** How long the fade to and from black takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Cutscene")
	float FadeTime = 0.6f;

private:
	void AdvanceLine();
	void FinishCutscene();
	void SetPlayerInputEnabled(bool bEnabled);

	UPROPERTY()
	TArray<FText> CutsceneLines;

	int32 LineIndex = 0;
	float LineDuration = 3.5f;
	float StartTime = 0.0f;
	float TotalDuration = 0.0f;
	bool bPlaying = false;

	FTimerHandle LineTimer;
};
