#include "Dialogue/Events/DialogueEvent_Task.h"

#include "Task/TaskModule.h"

bool UDialogueEvent_Task::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return ApplyTaskAction(InContext.Target);
}

void UDialogueEvent_Task::RecieveEventTriggered_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	ApplyTaskAction(NPCActor);
}

bool UDialogueEvent_Task::ApplyTaskAction(AActor* InTarget) const
{
	if (!UTaskModule::IsValid()) return false;
	UTaskModule& Module = UTaskModule::Get();
	UTaskBase* RuntimeTask = Action == EDialogueTaskAction::Accept ? Module.EnsureTask(Task) : Module.ResolveTask(Task);
	if (!RuntimeTask) return false;
	const ETaskState PreviousState = RuntimeTask->TaskState;
	switch (Action)
	{
		case EDialogueTaskAction::Accept: Module.EnterTask(RuntimeTask, true); break;
		case EDialogueTaskAction::Execute: Module.ExecuteTask(RuntimeTask); break;
		case EDialogueTaskAction::Complete: Module.CompleteTask(RuntimeTask); break;
		case EDialogueTaskAction::TurnIn: return Module.TurnInTask(RuntimeTask, InTarget);
		case EDialogueTaskAction::Track:
		{
			if (RuntimeTask->IsLeaved()) return false;
			Module.SetCurrentTask(RuntimeTask);
			return Module.GetCurrentTask() == RuntimeTask;
		}
	}
	return RuntimeTask->TaskState != PreviousState;
}
