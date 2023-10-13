// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpRaw.h"

#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpRaw::AAbilityPickUpRaw()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpRaw::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpRaw::GetMeshComponent() const
{
	return MeshComponent;
}
