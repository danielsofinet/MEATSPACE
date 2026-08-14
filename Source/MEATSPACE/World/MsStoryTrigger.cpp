#include "World/MsStoryTrigger.h"

#include "Character/MsCharacter.h"
#include "Combat/MsCombatTypes.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
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
			// Rewards wait for the cutscene to end. Handing someone a gun during a fade to
			// black means the moment they get it is a moment they cannot see.
			Cutscenes->OnCutsceneFinished.AddDynamic(this, &AMsStoryTrigger::HandleCutsceneFinished);
			Cutscenes->PlayCutscene(CutsceneLines, SecondsPerLine);

			OnStoryTriggered.Broadcast(this);
			return;
		}
	}

	// No cutscene - hand them out immediately.
	ApplyRewards();

	OnStoryTriggered.Broadcast(this);
}

void AMsStoryTrigger::HandleCutsceneFinished()
{
	if (UWorld* World = GetWorld())
	{
		if (UMsCutsceneSubsystem* Cutscenes = World->GetSubsystem<UMsCutsceneSubsystem>())
		{
			Cutscenes->OnCutsceneFinished.RemoveDynamic(this, &AMsStoryTrigger::HandleCutsceneFinished);
		}
	}

	ApplyRewards();
}

void AMsStoryTrigger::ApplyRewards()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (AMsCharacter* PlayerCharacter = Cast<AMsCharacter>(PC->GetPawn()))
		{
			if (bGrantSword)
			{
				PlayerCharacter->UnlockWeapon(EMsWeaponSlot::Sword, /*bEquipImmediately=*/true);
			}

			if (bGrantGun)
			{
				PlayerCharacter->UnlockWeapon(EMsWeaponSlot::Gun, /*bEquipImmediately=*/false);
			}

			if (GrantShieldAmount > 0.0f)
			{
				PlayerCharacter->UnlockShield(GrantShieldAmount);
			}
		}
	}

	if (!ObjectiveAfterCutscene.IsEmpty())
	{
		if (UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>())
		{
			Objectives->SetObjective(ObjectiveAfterCutscene, ObjectiveTargetAfterCutscene);
		}
	}
}
