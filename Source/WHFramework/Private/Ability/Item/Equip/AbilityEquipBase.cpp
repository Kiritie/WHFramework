// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Equip/AbilityEquipBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

AAbilityEquipBase::AAbilityEquipBase()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
}

void AAbilityEquipBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);
}

void AAbilityEquipBase::OnAssemble_Implementation()
{
	
}

void AAbilityEquipBase::OnDischarge_Implementation()
{
	
}
