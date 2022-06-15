// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Actor/PickUp/PickUpEquip.h"

#include "Ability/Item/Equip/AbilityEquipDataBase.h"
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

void APickUpEquip::Initialize(FAbilityItem InItem, bool bPreview /*= false*/)
{
	Super::Initialize(InItem, bPreview);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(Item.GetData<UAbilityEquipDataBase>().EquipMesh);
}

void APickUpEquip::OnPickUp(IPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}
