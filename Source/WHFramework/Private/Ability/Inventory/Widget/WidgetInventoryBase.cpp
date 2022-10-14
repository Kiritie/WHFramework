#include "Ability/Inventory/Widget/WidgetInventoryBase.h"
#include "Ability/Inventory/InventoryAgentInterface.h"
#include "Ability/Inventory/Widget/WidgetInventorySlotBase.h"

UWidgetInventoryBase::UWidgetInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UWidgetInventoryBase::OnInitialize_Implementation(AActor* InOwner)
{
	Super::OnInitialize_Implementation(InOwner);
}

void UWidgetInventoryBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}

UInventory* UWidgetInventoryBase::GetInventory() const
{
	if(IInventoryAgentInterface* InventoryAgent = Cast<IInventoryAgentInterface>(OwnerActor))
	{
		return InventoryAgent->GetInventory();
	}
	return nullptr;
}

TArray<UWidgetInventorySlotBase*> UWidgetInventoryBase::GetSplitUISlots(ESplitSlotType InSplitSlotType) const
{
	if(UISlotDatas.Contains(InSplitSlotType))
	{
		return UISlotDatas[InSplitSlotType].Slots;
	}
	return TArray<UWidgetInventorySlotBase*>();
}