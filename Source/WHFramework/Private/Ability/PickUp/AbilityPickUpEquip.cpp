// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpEquip.h"

#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpEquip::AAbilityPickUpEquip()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpEquip::Initialize_Implementation(FAbilityItem InItem)
{
	Super::Initialize_Implementation(InItem);
}

void AAbilityPickUpEquip::OnPickUp_Implementation(const TScriptInterface<IAbilityPickerInterface>& InPicker)
{
	Super::OnPickUp_Implementation(InPicker);

}

UMeshComponent* AAbilityPickUpEquip::GetMeshComponent() const
{
	return MeshComponent;
}
