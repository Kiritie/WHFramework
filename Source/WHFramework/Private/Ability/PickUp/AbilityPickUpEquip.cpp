// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpEquip.h"

#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpEquip::AAbilityPickUpEquip()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpEquip::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}

UMeshComponent* AAbilityPickUpEquip::GetMeshComponent() const
{
	return MeshComponent;
}
