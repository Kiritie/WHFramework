// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

AAbilityPickUpBase::AAbilityPickUpBase()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionProfileName(TEXT("PickUp"));
	BoxComponent->SetBoxExtent(FVector(10, 10, 10));
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityPickUpBase::OnOverlap);

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent"));
	RotatingComponent->RotationRate = FRotator(0, 180, 0);

	Item = FAbilityItem::Empty;
	Container = nullptr;
}

void AAbilityPickUpBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilityPickUpBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	Item = FAbilityItem::Empty;
	Container = nullptr;
}

void AAbilityPickUpBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
}

FSaveData* AAbilityPickUpBase::ToData()
{
	static FPickUpSaveData SaveData;
	SaveData =  FPickUpSaveData();

	SaveData.Item = Item;
	SaveData.Location = GetActorLocation();

	return &SaveData;
}

void AAbilityPickUpBase::Initialize_Implementation(FAbilityItem InItem)
{
	Item = InItem;
}

void AAbilityPickUpBase::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(!Item.IsValid()) return;

	if (OtherActor)
	{
		if (OtherActor->GetClass()->ImplementsInterface(UAbilityPickerInterface::StaticClass()))
		{
			OnPickUp(OtherActor);
		}
	}
}

void AAbilityPickUpBase::OnPickUp_Implementation(const TScriptInterface<IAbilityPickerInterface>& InPicker)
{
	if(InPicker)
	{
		InPicker->PickUp(this);
	}
	UObjectPoolModuleBPLibrary::DespawnObject(this);
}
