#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"

UWidgetAbilityInventoryBase::UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UWidgetAbilityInventoryBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetAbilityInventoryBase::OnRefresh()
{
	Super::OnRefresh();
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
