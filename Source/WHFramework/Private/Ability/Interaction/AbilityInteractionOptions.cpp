#include "Ability/Interaction/AbilityInteractionOptions.h"

#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
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
	AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InContext.Target);
	const IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(InContext.Interactor);
	return PickUp && Picker && PickUp->GetItem().IsValid() && !Picker->IsAutoPickUp();
}

bool UInteractionAction_AbilityPickUp::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InContext.Target);
	IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(InContext.Interactor);
	if (!PickUp || !Picker || !PickUp->GetItem().IsValid() || Picker->IsAutoPickUp()) return false;
	PickUp->OnPickUp(Picker);
	return true;
}

UInteractionOption_AbilityPickUp::UInteractionOption_AbilityPickUp()
{
	OptionTag = GameplayTags::Interaction_Option_PickUp;
	DisplayName = NSLOCTEXT("Interaction", "PickUp", "拾取");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_AbilityPickUp>(TEXT("PickUpCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_AbilityPickUp>(TEXT("PickUpAction")));
}
