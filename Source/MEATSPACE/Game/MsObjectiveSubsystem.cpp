#include "Game/MsObjectiveSubsystem.h"

void UMsObjectiveSubsystem::SetObjective(const FText& Text, AActor* Target)
{
	ObjectiveText = Text;
	ObjectiveTarget = Target;

	OnObjectiveChanged.Broadcast();
}

void UMsObjectiveSubsystem::ClearObjective()
{
	ObjectiveText = FText::GetEmpty();
	ObjectiveTarget = nullptr;

	OnObjectiveChanged.Broadcast();
}
