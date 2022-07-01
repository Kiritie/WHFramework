// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpProp.h"

#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
AAbilityPickUpProp::AAbilityPickUpProp()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AAbilityPickUpProp::BeginPlay()
{
	Super::BeginPlay();

}

void AAbilityPickUpProp::Initialize(FAbilityItem InItem)
{
	Super::Initialize(InItem);
	Cast<UStaticMeshComponent>(MeshComponent)->SetStaticMesh(Item.GetData<UAbilityPropDataBase>().PropMesh);
}

void AAbilityPickUpProp::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);

}
