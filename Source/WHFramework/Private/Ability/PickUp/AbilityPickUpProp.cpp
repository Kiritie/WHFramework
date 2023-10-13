// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpProp.h"

#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpProp::AAbilityPickUpProp()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpProp::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpProp::GetMeshComponent() const
{
	return MeshComponent;
}
