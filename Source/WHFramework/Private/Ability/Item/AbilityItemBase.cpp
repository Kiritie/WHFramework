// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/AbilityItemBase.h"

#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Common/CommonStatics.h"

// Sets default values
AAbilityItemBase::AAbilityItemBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Item = FAbilityItem::Empty;
	OwnerActor = nullptr;
}

void AAbilityItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	OwnerActor = Cast<AActor>(InOwner);
	
	if(InParams.IsValidIndex(0))
	{
		Item = InParams[0].GetPointerValueRef<FAbilityItem>();
		Item.Payload = this;
	}
}

void AAbilityItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

bool AAbilityItemBase::Active_Implementation()
{
	if(Item.GetPayload<UAbilityInventorySlotBase>())
	{
		return Item.GetPayload<UAbilityInventorySlotBase>()->ActiveItem();
	}
	return false;
}

void AAbilityItemBase::Deactive_Implementation()
{
	if(Item.GetPayload<UAbilityInventorySlotBase>())
	{
		Item.GetPayload<UAbilityInventorySlotBase>()->DeactiveItem();
	}
}
