#include "Dialogue/Conditions/DialogueCondition_Task.h"

#include "Task/TaskModule.h"

bool UDialogueCondition_Task::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	if (!UTaskModule::IsValid()) return false;
	if (UTaskBase* RuntimeTask = UTaskModule::Get().ResolveTask(Task)) return RuntimeTask->GetTaskStage() == RequiredStage;
	return false;
}
