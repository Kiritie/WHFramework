#include "Ability/Interaction/AbilityInteractionOptions.h"

#include "Ability/Inventory/InventoryPickupActor.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "GameFramework/PlayerController.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"

bool UInteractionCondition_AbilityRevive::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const IAbilityVitalityInterface* Target = Cast<IAbilityVitalityInterface>(InContext.Target);
	return Target && Cast<IAbilityVitalityInterface>(InContext.Interactor) && Target->IsDead();
}

bool UInteractionAction_AbilityRevive::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	IAbilityVitalityInterface* Target = Cast<IAbilityVitalityInterface>(InContext.Target);
	IAbilityVitalityInterface* Rescuer = Cast<IAbilityVitalityInterface>(InContext.Interactor);
	if (!Target || !Rescuer || !Target->IsDead()) return false;
	Target->Revive(Rescuer);
	return true;
}

UInteractionOption_AbilityRevive::UInteractionOption_AbilityRevive()
{
	OptionTag = GameplayTags::Interaction_Option_Revive;
	DisplayName = NSLOCTEXT("Interaction", "Revive", "复活");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_AbilityRevive>(TEXT("ReviveCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_AbilityRevive>(TEXT("ReviveAction")));
}

bool UInteractionCondition_AbilityPickUp::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
    const AInventoryPickupActor* Pickup = Cast<AInventoryPickupActor>(InContext.Target);
    const APlayerController* Player = InContext.Player;
    return Pickup && Pickup->GetInventoryId().IsValid() && Pickup->GetPublicCount() > 0 && !Pickup->GetPickupAction().IsNone()
        && Player && Player->FindComponentByClass<UInventoryNetworkComponent>();
}

bool UInteractionAction_AbilityPickUp::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
    const AInventoryPickupActor* Pickup = Cast<AInventoryPickupActor>(InContext.Target);
    UInventoryNetworkComponent* Network = InContext.Player ? InContext.Player->FindComponentByClass<UInventoryNetworkComponent>() : nullptr;
    if(!Pickup || !Network || Pickup->GetPublicCount() <= 0 || Pickup->GetPickupAction().IsNone()) return false;
    FInventoryClientRequest Request;
    Request.DomainAction = Pickup->GetPickupAction();
    Request.SourceInventoryId = Network->GetOwnerInventoryId();
    Request.TargetEntityId = Pickup->GetInventoryId();
    Request.Quantity = Pickup->GetPublicCount();
    Request.bAllowPartial = true;
    const bool bSubmitted = Network->SubmitRequest(MoveTemp(Request)) != 0;
    if(!bSubmitted) OutReason = NSLOCTEXT("Interaction", "PickupUnavailable", "拾取请求暂不可提交。");
    return bSubmitted;
}

UInteractionOption_AbilityPickUp::UInteractionOption_AbilityPickUp()
{
	OptionTag = GameplayTags::Interaction_Option_PickUp;
	DisplayName = NSLOCTEXT("Interaction", "PickUp", "拾取");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_AbilityPickUp>(TEXT("PickUpCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_AbilityPickUp>(TEXT("PickUpAction")));
}

