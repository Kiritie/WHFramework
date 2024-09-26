// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Equip/AbilityEquipBase.h"

AAbilityEquipBase::AAbilityEquipBase()
{
}

void AAbilityEquipBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);
}

void AAbilityEquipBase::OnAssemble_Implementation()
{
	
}

void AAbilityEquipBase::OnDischarge_Implementation()
{
	
}

UMeshComponent* AAbilityEquipBase::GetMeshComponent() const
{
	return FindComponentByClass<UMeshComponent>();
}
