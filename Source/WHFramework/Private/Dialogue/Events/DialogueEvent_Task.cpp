#include "Dialogue/Events/DialogueEvent_Task.h"
#include "Task/TaskExecutionTypes.h"
#include "Common/InteractionTypes.h"

#include "Task/TaskAgentInterface.h"
#include "Task/TaskComponent.h"
#include "Task/TaskModule.h"
#include "GameFramework/PlayerController.h"
#include "Scene/Actor/SceneActorInterface.h"

void UDialogueEvent_Task::PostLoad()
{
	Super::PostLoad();
	ResolvedTaskAsset = Task.Asset.LoadSynchronous();
}

bool UDialogueEvent_Task::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return ApplyTaskAction(InContext.Target, InContext.Interactor);
}

void UDialogueEvent_Task::ReceiveEventTriggered_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	ApplyTaskAction(NPCActor, ConsideringPlayer ? ConsideringPlayer->GetPawn() : nullptr);
}

bool UDialogueEvent_Task::ApplyTaskAction(AActor* InTarget, AActor* InInteractor) const
{
	UWorld* World = InInteractor ? InInteractor->GetWorld() : nullptr;
	if (!World) return false;
	UTaskModule* Tasks = UTaskModule::GetPtr(World);
	if (!Tasks) return false;
	UTaskModule& Module = *Tasks;
	UTaskComponent* Component = nullptr;
	if(ITaskAgentInterface* Agent = Cast<ITaskAgentInterface>(InTarget)) Component = Agent->GetTaskComponent();
	if(Action == EDialogueTaskAction::Accept && Component)
	{
		UTaskAsset* Source = ResolvedTaskAsset ? ResolvedTaskAsset : Task.Asset.Get();
		return Component->AcceptTask(Source, Task.TaskGUID, InInteractor) != nullptr;
	}
	const FGuid PlayerId = InInteractor && InInteractor->Implements<USceneActorInterface>() ? ISceneActorInterface::Execute_GetActorID(InInteractor) : FGuid();
	if (!PlayerId.IsValid()) return false;
	UTaskBase* RuntimeTask = Component ? Component->ResolveTask(InInteractor, Task) : Module.ResolveTask(Task);
	if (!RuntimeTask) return false;
	if (!RuntimeTask->GetTaskAsset() || RuntimeTask->GetTaskAsset()->OwnerActorID != PlayerId) return false;
	const ETaskState PreviousState = RuntimeTask->TaskState;
	switch (Action)
	{
		case EDialogueTaskAction::Accept: Module.EnterTask(RuntimeTask, true); break;
		case EDialogueTaskAction::Execute: Module.ExecuteTask(RuntimeTask); break;
		case EDialogueTaskAction::Complete: Module.CompleteTask(RuntimeTask); break;
		case EDialogueTaskAction::TurnIn: return Component ? Component->TurnInTask(InInteractor, RuntimeTask) : Module.TurnInTask(RuntimeTask, InTarget);
		case EDialogueTaskAction::Track:
		{
			if (RuntimeTask->IsLeaved()) return false;
			Module.SetCurrentTask(PlayerId, RuntimeTask);
			return PlayerId.IsValid() && Module.GetCurrentTask(PlayerId) == RuntimeTask;
		}
		case EDialogueTaskAction::CompleteAndTurnIn:
		{
			UTaskBase* RootTask = RuntimeTask->RootTask ? RuntimeTask->RootTask : RuntimeTask;
			Module.CompleteTask(RuntimeTask);
			if (!RootTask || RootTask->TaskState != ETaskState::Completed || RootTask->TaskExecuteResult != ETaskExecuteResult::Succeed) return false;
			return Component ? Component->TurnInTask(InInteractor, RootTask) : Module.TurnInTask(RootTask, InTarget);
		}
	}
	return RuntimeTask->TaskState != PreviousState;
}
