// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Actor/PickUp/PickUpProp.h"

#include "Ability/Item/Prop/PropDataBase.h"
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

void APickUpProp::Initialize(FAbilityItem InItem, bool bPreview /*= false*/)
{
	Super::Initialize(InItem, bPreview);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(Item.GetData<UPropDataBase>()->PropMesh);
}

void APickUpProp::OnPickUp(IPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}
