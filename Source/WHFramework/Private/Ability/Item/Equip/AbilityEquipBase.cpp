// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Equip/AbilityEquipBase.h"

AAbilityEquipBase::AAbilityEquipBase()
{
}

void AAbilityEquipBase::OnAssemble_Implementation()
{
	
}

void AAbilityEquipBase::OnDisassemble_Implementation()
{
	
}

UMeshComponent* AAbilityEquipBase::GetMeshComponent() const
{
	return FindComponentByClass<UMeshComponent>();
}
