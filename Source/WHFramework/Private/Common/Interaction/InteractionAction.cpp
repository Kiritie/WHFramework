#include "Common/Interaction/InteractionAction.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

bool UInteractionAction::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return true;
}

UWorld* UInteractionAction::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;
	if (const UActorComponent* Component = GetTypedOuter<UActorComponent>()) return Component->GetWorld();
	if (const AActor* Actor = GetTypedOuter<AActor>()) return Actor->GetWorld();
	return nullptr;
}
