// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpProp.h"

#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpProp::AAbilityPickUpProp()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpProp::Initialize_Implementation(FAbilityItem InItem)
{
	Super::Initialize_Implementation(InItem);
}

void AAbilityPickUpProp::OnPickUp_Implementation(const TScriptInterface<IAbilityPickerInterface>& InPicker)
{
	Super::OnPickUp_Implementation(InPicker);
}

UMeshComponent* AAbilityPickUpProp::GetMeshComponent() const
{
	return MeshComponent;
}
