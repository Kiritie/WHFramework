#include "Dialogue/Conditions/DialogueCondition_Task.h"
#include "Task/TaskExecutionTypes.h"

#include "Task/TaskAgentInterface.h"
#include "Task/TaskComponent.h"
#include "Task/TaskModule.h"
#include "GameFramework/PlayerController.h"
#include "Scene/Actor/SceneActorInterface.h"

bool UDialogueCondition_Task::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	UWorld* World = ConsideringPlayer ? ConsideringPlayer->GetWorld() : nullptr;
	if (!World) return false;
	UTaskModule* Tasks = UTaskModule::GetPtr(World);
	AActor* Interactor = ConsideringPlayer->GetPawn();
	if (!Tasks || !Interactor || !Interactor->Implements<USceneActorInterface>()) return false;
	const FGuid PlayerId = ISceneActorInterface::Execute_GetActorID(Interactor);
	if (!PlayerId.IsValid()) return false;
	if(ITaskAgentInterface* Agent = Cast<ITaskAgentInterface>(NPCActor))
	{
		if(UTaskComponent* Component = Agent->GetTaskComponent())
		{
			if(UTaskBase* RuntimeTask = Component->ResolveTask(Interactor, Task)) return RuntimeTask->GetTaskStage() == RequiredStage;
			if(RequiredStage == ETaskStage::Available)
			{
				for(UTaskAsset* Offer : Component->GetOfferedAssets(Interactor))
				{
					if(Task.Asset.IsNull() || Task.Asset.ToSoftObjectPath() == FSoftObjectPath(Offer)) return true;
				}
			}
			return false;
		}
	}
	if (UTaskBase* RuntimeTask = Tasks->ResolveTask(Task))
		return RuntimeTask->GetTaskAsset() && RuntimeTask->GetTaskAsset()->OwnerActorID == PlayerId && RuntimeTask->GetTaskStage() == RequiredStage;
	return false;
}
