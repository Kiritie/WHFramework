#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class AActor;

namespace SceneActorIdentity
{
	void InitializeCreated(AActor& Actor, FGuid& Identity);
	void InitializeEditorDuplicate(AActor& Actor, FGuid& Identity, EDuplicateMode::Type DuplicateMode);
	void InitializeEditorImport(AActor& Actor, FGuid& Identity);
	bool Assign(AActor& Actor, FGuid& Identity, const FGuid& NewIdentity);
	void QueueReplication(AActor& Actor, FTSTicker::FDelegateHandle& RetryTicker);
	void CancelReplication(FTSTicker::FDelegateHandle& RetryTicker);
}
