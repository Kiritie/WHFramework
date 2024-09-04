#include "Ability/Inventory/Widget/WidgetInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Widget/WidgetInventorySlotBase.h"

UWidgetInventoryBase::UWidgetInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UWidgetInventoryBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetInventoryBase::OnRefresh()
{
	Super::OnRefresh();
}

UAbilityInventoryBase* UWidgetInventoryBase::GetInventory() const
{
	if(const IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(OwnerObject))
	{
		return InventoryAgent->GetInventory();
	}
	return nullptr;
}

TArray<UWidgetInventorySlotBase*> UWidgetInventoryBase::GetSplitUISlots(ESlotSplitType InSplitType) const
{
	if(UISlotDatas.Contains(InSplitType))
	{
		return UISlotDatas[InSplitType].Slots;
	}
	return TArray<UWidgetInventorySlotBase*>();
}
