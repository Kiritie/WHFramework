// Fill out your copyright notice in the Description page of Project Settings.


#include "PickUp/PickUpEquip.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
APickUpEquip::APickUpEquip()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

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
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(GetEquipData().EquipMesh);
}

FEquipData APickUpEquip::GetEquipData()
{
	return UDWHelper::LoadEquipData(Item.ID);
}

void APickUpEquip::OnPickUp(AAbilityCharacterBase* InPicker)
{
	Super::OnPickUp(InPicker);

}

// Called every frame
void APickUpEquip::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
