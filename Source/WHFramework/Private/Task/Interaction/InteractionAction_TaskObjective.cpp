#include "Task/Interaction/InteractionAction_TaskObjective.h"

#include "Task/TaskModule.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "GameFramework/Actor.h"

bool UInteractionAction_TaskObjective::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	AActor* Interactor = InContext.Interactor;
	UTaskModule* Tasks = IsValid(Interactor) ? UTaskModule::GetPtr(Interactor->GetWorld()) : nullptr;
	if (!Tasks || !Interactor->HasAuthority() || !Interactor->Implements<USceneActorInterface>() ||
		!IsValid(InContext.Target) || InContext.Target->GetWorld() != Interactor->GetWorld() || !EventTag.IsValid() || Count < 1) return false;
	const FGuid OwnerId = ISceneActorInterface::Execute_GetActorID(Interactor);
	if (!OwnerId.IsValid()) return false;
	Tasks->ReportTaskEvent(EventTag, TargetTag, Count, FPrimaryAssetId(), NAME_None, OwnerId);
	return true;
}
