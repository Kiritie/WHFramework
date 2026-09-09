#include "Dialogue/Events/DialogueEvent_Task.h"

#include "Task/TaskAgentInterface.h"
#include "Task/TaskComponent.h"
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
	UTaskComponent* Component = nullptr;
	if(ITaskAgentInterface* Agent = Cast<ITaskAgentInterface>(InTarget)) Component = Agent->GetTaskComponent();
	if(Action == EDialogueTaskAction::Accept && Component)
	{
		UTaskAsset* Source = Task.Asset.IsNull() ? nullptr : Task.Asset.LoadSynchronous();
		return Component->AcceptTask(Source, Task.TaskGUID) != nullptr;
	}
	UTaskBase* RuntimeTask = Component ? Component->ResolveTask(Task) : (Action == EDialogueTaskAction::Accept ? Module.EnsureTask(Task) : Module.ResolveTask(Task));
	if (!RuntimeTask) return false;
	const ETaskState PreviousState = RuntimeTask->TaskState;
	switch (Action)
	{
		case EDialogueTaskAction::Accept: Module.EnterTask(RuntimeTask, true); break;
		case EDialogueTaskAction::Execute: Module.ExecuteTask(RuntimeTask); break;
		case EDialogueTaskAction::Complete: Module.CompleteTask(RuntimeTask); break;
		case EDialogueTaskAction::TurnIn: return Component ? Component->TurnInTask(RuntimeTask) : Module.TurnInTask(RuntimeTask, InTarget);
		case EDialogueTaskAction::Track:
		{
			if (RuntimeTask->IsLeaved()) return false;
			Module.SetCurrentTask(RuntimeTask);
			return Module.GetCurrentTask() == RuntimeTask;
		}
		case EDialogueTaskAction::CompleteAndTurnIn:
		{
			UTaskBase* RootTask = RuntimeTask->RootTask ? RuntimeTask->RootTask : RuntimeTask;
			Module.CompleteTask(RuntimeTask);
			if (!RootTask || RootTask->TaskState != ETaskState::Completed || RootTask->TaskExecuteResult != ETaskExecuteResult::Succeed) return false;
			return Component ? Component->TurnInTask(RootTask) : Module.TurnInTask(RootTask, InTarget);
		}
	}
	return RuntimeTask->TaskState != PreviousState;
}
