// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Prop/AbilityPropBase.h"

// Sets default values
AAbilityPropBase::AAbilityPropBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AAbilityPropBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AAbilityPropBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAbilityPropBase::Initialize(AAbilityCharacterBase* InOwnerCharacter)
{
	Super::Initialize(InOwnerCharacter);
}
