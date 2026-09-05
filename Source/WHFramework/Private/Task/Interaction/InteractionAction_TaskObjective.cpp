#include "Task/Interaction/InteractionAction_TaskObjective.h"

#include "Task/TaskModule.h"

bool UInteractionAction_TaskObjective::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	if (!UTaskModule::IsValid() || !EventTag.IsValid() || Count < 1) return false;
	UTaskModule::Get().ReportTaskEvent(EventTag, TargetTag, Count);
	return true;
}
