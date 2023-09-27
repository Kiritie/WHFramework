#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"

UWidgetAbilityInventoryBase::UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UWidgetAbilityInventoryBase::OnInitialize_Implementation(UObject* InOwner)
{
	Super::OnInitialize_Implementation(InOwner);
}

void UWidgetAbilityInventoryBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}

UAbilityInventoryBase* UWidgetAbilityInventoryBase::GetInventory() const
{
	if(const IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(OwnerObject))
	{
		return InventoryAgent->GetInventory();
	}
	return nullptr;
}

TArray<UWidgetAbilityInventorySlotBase*> UWidgetAbilityInventoryBase::GetSplitUISlots(ESlotSplitType InSplitType) const
{
	if(UISlotDatas.Contains(InSplitType))
	{
		return UISlotDatas[InSplitType].Slots;
	}
	return TArray<UWidgetAbilityInventorySlotBase*>();
}
