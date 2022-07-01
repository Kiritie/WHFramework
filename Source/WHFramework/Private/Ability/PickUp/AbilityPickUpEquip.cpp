// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpEquip.h"

#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
AAbilityPickUpEquip::AAbilityPickUpEquip()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AAbilityPickUpEquip::BeginPlay()
{
	Super::BeginPlay();

}

void AAbilityPickUpEquip::Initialize(FAbilityItem InItem)
{
	Super::Initialize(InItem);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(Item.GetData<UAbilityEquipDataBase>().EquipMesh);
}

void AAbilityPickUpEquip::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}
