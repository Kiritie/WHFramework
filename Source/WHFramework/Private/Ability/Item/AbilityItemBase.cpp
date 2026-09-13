// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/AbilityItemBase.h"

#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Common/CommonModuleStatics.h"

// Sets default values
AAbilityItemBase::AAbilityItemBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Item = FAbilityItem::Empty;
	OwnerActor = nullptr;
}

void AAbilityItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	if(const FAbilityItemSpawnParameter* Param = InParam.GetPtr<FAbilityItemSpawnParameter>())
	{
		OwnerActor = Param->Owner;
		Item = Param->Item;
	}
}

void AAbilityItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
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
