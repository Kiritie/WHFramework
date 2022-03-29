// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Object/PickUp/PickUpEquip.h"

#include "Ability/Item/Equip/EquipAssetBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
APickUpEquip::APickUpEquip()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void APickUpEquip::BeginPlay()
{
	Super::BeginPlay();

}

void APickUpEquip::Initialize(FItem InItem, bool bPreview /*= false*/)
{
	Super::Initialize(InItem, bPreview);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(Item.GetData<UEquipAssetBase>()->EquipMesh);
}

void APickUpEquip::OnPickUp(IPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}