// Fill out your copyright notice in the Description page of Project Settings.


#include "PickUp/PickUpProp.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
APickUpProp::APickUpProp()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void APickUpProp::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void APickUpProp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APickUpProp::Initialize(FItem InItem, bool bPreview /*= false*/)
{
	Super::Initialize(InItem, bPreview);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(GetPropData().PropMesh);
}

void APickUpProp::OnPickUp(AAbilityCharacterBase* InPicker)
{
	Super::OnPickUp(InPicker);

}

FPropData APickUpProp::GetPropData()
{
	return UDWHelper::LoadPropData(Item.ID);
}
