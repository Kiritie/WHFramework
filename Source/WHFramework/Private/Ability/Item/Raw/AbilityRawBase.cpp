// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Raw/AbilityRawBase.h"

AAbilityRawBase::AAbilityRawBase()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
}

void AAbilityRawBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);
}
