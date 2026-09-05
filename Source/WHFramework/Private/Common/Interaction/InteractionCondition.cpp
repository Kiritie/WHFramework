#include "Common/Interaction/InteractionCondition.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

bool UInteractionCondition::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return true;
}

UWorld* UInteractionCondition::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;
	if (const UActorComponent* Component = GetTypedOuter<UActorComponent>()) return Component->GetWorld();
	if (const AActor* Actor = GetTypedOuter<AActor>()) return Actor->GetWorld();
	return nullptr;
}
