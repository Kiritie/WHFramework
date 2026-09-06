#include "Task/Interaction/InteractionAction_Task.h"

#include "Task/TaskAgentInterface.h"
#include "Task/TaskComponent.h"

bool UInteractionAction_Task::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const ITaskAgentInterface* Agent = Cast<ITaskAgentInterface>(InContext.Target);
	UTaskComponent* Component = Agent ? Agent->GetTaskComponent() : nullptr;
	if(!Component)
	{
		OutReason = NSLOCTEXT("Interaction", "TaskAgentUnavailable", "该角色无法处理任务。");
		return false;
	}
	if(UTaskBase* Task = Component->GetTask(ETaskStage::Deliverable))
	{
		if(Component->TurnInTask(Task)) return true;
		OutReason = NSLOCTEXT("Interaction", "TaskTurnInFailed", "任务暂时无法交付。");
		return false;
	}
	if(Component->AcceptTask()) return true;
	OutReason = NSLOCTEXT("Interaction", "TaskAcceptFailed", "当前没有可接取的任务。");
	return false;
}
