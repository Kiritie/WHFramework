// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Prop/AbilityPropBase.h"

AAbilityPropBase::AAbilityPropBase()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
}

void AAbilityPropBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);
}
