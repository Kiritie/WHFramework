// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpRaw.h"

#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpRaw::AAbilityPickUpRaw()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpRaw::Initialize_Implementation(FAbilityItem InItem)
{
	Super::Initialize_Implementation(InItem);
}

void AAbilityPickUpRaw::OnPickUp_Implementation(const TScriptInterface<IAbilityPickerInterface>& InPicker)
{
	Super::OnPickUp_Implementation(InPicker);
}

UMeshComponent* AAbilityPickUpRaw::GetMeshComponent() const
{
	return MeshComponent;
}
