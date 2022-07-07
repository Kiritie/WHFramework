// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/AbilityItemBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Global/GlobalBPLibrary.h"

// Sets default values
AAbilityItemBase::AAbilityItemBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Item = FAbilityItem::Empty;
	OwnerCharacter = nullptr;
}

// Called when the game starts or when spawned
void AAbilityItemBase::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AAbilityItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAbilityItemBase::Initialize(AAbilityCharacterBase* InOwnerCharacter)
{
	OwnerCharacter = InOwnerCharacter;
}

void AAbilityItemBase::Initialize(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Initialize(InOwnerCharacter);
	Item = InItem;
}
