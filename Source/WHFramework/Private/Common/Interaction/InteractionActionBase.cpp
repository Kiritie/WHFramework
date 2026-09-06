#include "Common/Interaction/InteractionActionBase.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

bool UInteractionActionBase::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return true;
}

UWorld* UInteractionActionBase::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;
	if (const UActorComponent* Component = GetTypedOuter<UActorComponent>()) return Component->GetWorld();
	if (const AActor* Actor = GetTypedOuter<AActor>()) return Actor->GetWorld();
	return nullptr;
}

void UInteractionActionBase::Finish_Implementation(const FInteractionContext& InContext, EInteractionActionState InState) const
{
}
