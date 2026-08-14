#include "World/MsStoryTrigger.h"

#include "Character/MsCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Game/MsCutsceneSubsystem.h"
#include "Game/MsObjectiveSubsystem.h"

AMsStoryTrigger::AMsStoryTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(300.0f, 300.0f, 300.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	SetRootComponent(TriggerBox);
}

void AMsStoryTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMsStoryTrigger::HandleTriggerOverlap);
	}
}

void AMsStoryTrigger::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Players only - a clanker chasing you through the doorway must not start the cutscene.
	if (!Cast<AMsCharacter>(OtherActor))
	{
		return;
	}

	Fire();
}

void AMsStoryTrigger::Fire()
{
	if (bFired && bTriggerOnce)
	{
		return;
	}

	bFired = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!ObjectiveOnTrigger.IsEmpty())
	{
		if (UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>())
		{
			Objectives->SetObjective(ObjectiveOnTrigger, ObjectiveTarget);
		}
	}

	if (CutsceneLines.Num() > 0)
	{
		if (UMsCutsceneSubsystem* Cutscenes = World->GetSubsystem<UMsCutsceneSubsystem>())
		{
			Cutscenes->PlayCutscene(CutsceneLines, SecondsPerLine);
		}
	}

	OnStoryTriggered.Broadcast(this);
}
