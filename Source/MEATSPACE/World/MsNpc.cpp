#include "World/MsNpc.h"

#include "Character/MsCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/MsObjectiveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "Meatspace.Dialogue"

AMsNpc::AMsNpc()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	// Placeholder body until Daniel's character work lands.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

	Prompt = LOCTEXT("TalkPrompt", "Talk");
	RepeatPrompt = LOCTEXT("TalkAgainPrompt", "Talk again");
	SpeakerName = LOCTEXT("DefaultSpeaker", "Stranger");
}

const TArray<FText>& AMsNpc::ActiveLines() const
{
	if (bHasTalkedBefore && RepeatLines.Num() > 0)
	{
		return RepeatLines;
	}
	return Lines;
}

FText AMsNpc::GetPrompt() const
{
	if (bHasTalkedBefore && !RepeatPrompt.IsEmpty())
	{
		return RepeatPrompt;
	}
	return Prompt;
}

FText AMsNpc::GetCurrentLine() const
{
	const TArray<FText>& Active = ActiveLines();
	return Active.IsValidIndex(LineIndex) ? Active[LineIndex] : FText::GetEmpty();
}

void AMsNpc::Interact(AMsCharacter* Player)
{
	if (!Player || !bInteractable)
	{
		return;
	}

	if (!bTalking)
	{
		if (ActiveLines().Num() == 0)
		{
			// Nothing to say, but the outcome hooks should still fire - an NPC can be a pure
			// trigger with no dialogue at all.
			FinishDialogue(Player);
			return;
		}

		bTalking = true;
		LineIndex = 0;
		Player->BeginDialogue(this);
		return;
	}

	// Already talking: advance.
	++LineIndex;

	if (!ActiveLines().IsValidIndex(LineIndex))
	{
		FinishDialogue(Player);
	}
}

void AMsNpc::FinishDialogue(AMsCharacter* Player)
{
	bTalking = false;
	LineIndex = 0;
	bHasTalkedBefore = true;

	if (Player)
	{
		Player->EndDialogue();

		if (bGrantSword)
		{
			Player->UnlockWeapon(EMsWeaponSlot::Sword, /*bEquipImmediately=*/true);
		}

		if (bGrantGun)
		{
			Player->UnlockWeapon(EMsWeaponSlot::Gun, /*bEquipImmediately=*/false);
		}
	}

	if (!ObjectiveOnFinish.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>())
			{
				Objectives->SetObjective(ObjectiveOnFinish, ObjectiveTargetOnFinish);
			}
		}
	}

	if (bSingleUse)
	{
		bInteractable = false;
	}

	OnDialogueFinished.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
