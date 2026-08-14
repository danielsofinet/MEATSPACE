#include "Game/MsCutsceneSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UMsCutsceneSubsystem::PlayCutscene(const TArray<FText>& Lines, float SecondsPerLine)
{
	UWorld* World = GetWorld();
	if (!World || bPlaying || Lines.Num() == 0)
	{
		return;
	}

	CutsceneLines = Lines;
	LineIndex = 0;
	LineDuration = FMath::Max(SecondsPerLine, 0.2f);
	StartTime = World->GetTimeSeconds();
	TotalDuration = LineDuration * CutsceneLines.Num();
	bPlaying = true;

	SetPlayerInputEnabled(false);

	World->GetTimerManager().SetTimer(LineTimer, FTimerDelegate::CreateUObject(
		this, &UMsCutsceneSubsystem::AdvanceLine), LineDuration, true);
}

void UMsCutsceneSubsystem::AdvanceLine()
{
	++LineIndex;

	if (!CutsceneLines.IsValidIndex(LineIndex))
	{
		FinishCutscene();
	}
}

void UMsCutsceneSubsystem::SkipCutscene()
{
	if (bPlaying)
	{
		FinishCutscene();
	}
}

void UMsCutsceneSubsystem::FinishCutscene()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineTimer);
	}

	bPlaying = false;
	CutsceneLines.Reset();
	LineIndex = 0;

	SetPlayerInputEnabled(true);

	OnCutsceneFinished.Broadcast();
}

void UMsCutsceneSubsystem::SetPlayerInputEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (bEnabled)
			{
				Pawn->EnableInput(PC);
			}
			else
			{
				Pawn->DisableInput(PC);
			}
		}
	}
}

FText UMsCutsceneSubsystem::GetCurrentLine() const
{
	return CutsceneLines.IsValidIndex(LineIndex) ? CutsceneLines[LineIndex] : FText::GetEmpty();
}

float UMsCutsceneSubsystem::GetOverlayAlpha() const
{
	if (!bPlaying)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	if (!World || FadeTime <= 0.0f)
	{
		return 1.0f;
	}

	const float Elapsed = World->GetTimeSeconds() - StartTime;
	const float Remaining = TotalDuration - Elapsed;

	// Ramp up at the start, down at the end, solid black in between.
	const float FadeIn = FMath::Clamp(Elapsed / FadeTime, 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp(Remaining / FadeTime, 0.0f, 1.0f);

	return FMath::Min(FadeIn, FadeOut);
}
