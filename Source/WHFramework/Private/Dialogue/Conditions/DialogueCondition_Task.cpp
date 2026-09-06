#include "Dialogue/Conditions/DialogueCondition_Task.h"

#include "Task/TaskAgentInterface.h"
#include "Task/TaskComponent.h"
#include "Task/TaskModule.h"

bool UDialogueCondition_Task::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	if (!UTaskModule::IsValid()) return false;
	if(ITaskAgentInterface* Agent = Cast<ITaskAgentInterface>(NPCActor))
	{
		if(UTaskComponent* Component = Agent->GetTaskComponent())
		{
			if(UTaskBase* RuntimeTask = Component->ResolveTask(Task)) return RuntimeTask->GetTaskStage() == RequiredStage;
			if(RequiredStage == ETaskStage::Available)
			{
				for(UTaskAsset* Offer : Component->GetOfferedAssets())
				{
					if(Task.Asset.IsNull() || Task.Asset.ToSoftObjectPath() == FSoftObjectPath(Offer)) return true;
				}
			}
			return false;
		}
	}
	if (UTaskBase* RuntimeTask = UTaskModule::Get().ResolveTask(Task)) return RuntimeTask->GetTaskStage() == RequiredStage;
	return false;
}
