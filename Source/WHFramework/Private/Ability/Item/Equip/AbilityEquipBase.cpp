// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Equip/AbilityEquipBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Global/GlobalBPLibrary.h"

// Sets default values
AAbilityEquipBase::AAbilityEquipBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
	RootComponent = MeshComponent;
}

// Called when the game starts or when spawned
void AAbilityEquipBase::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AAbilityEquipBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAbilityEquipBase::Initialize(AAbilityCharacterBase* InOwnerCharacter)
{
	Super::Initialize(InOwnerCharacter);
}

void AAbilityEquipBase::SetVisible_Implementation(bool bVisible)
{
	GetRootComponent()->SetVisibility(bVisible, true);
}

void AAbilityEquipBase::OnAssemble_Implementation()
{
	
}

void AAbilityEquipBase::OnDischarge_Implementation()
{
	
}
