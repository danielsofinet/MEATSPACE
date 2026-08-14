#include "World/MsInteractable.h"

#include "Components/SceneComponent.h"

#define LOCTEXT_NAMESPACE "Meatspace.Interaction"

TArray<TWeakObjectPtr<AMsInteractable>> AMsInteractable::AllInteractables;

AMsInteractable::AMsInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// LOCTEXT rather than a raw string: this is player-facing, so it must be gatherable by
	// the localization pipeline from the very first line of text in the game.
	Prompt = LOCTEXT("DefaultPrompt", "Interact");
}

void AMsInteractable::BeginPlay()
{
	Super::BeginPlay();

	AllInteractables.Add(this);
}

void AMsInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AllInteractables.RemoveAll([this](const TWeakObjectPtr<AMsInteractable>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});

	Super::EndPlay(EndPlayReason);
}

const TArray<TWeakObjectPtr<AMsInteractable>>& AMsInteractable::GetAllInteractables()
{
	return AllInteractables;
}

void AMsInteractable::Interact(AMsCharacter* Player)
{
	// Subclasses do the work.
}

#undef LOCTEXT_NAMESPACE
