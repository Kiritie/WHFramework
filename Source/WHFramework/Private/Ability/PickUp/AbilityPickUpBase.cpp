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
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityPickUpBase::OnOverlap);

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent"));
	RotatingComponent->RotationRate = FRotator(0, 180, 0);

	Item = FAbilityItem::Empty;
	Container = nullptr;
}

void AAbilityPickUpBase::Initialize(FAbilityItem InItem)
{
	Item = InItem;
}

void AAbilityPickUpBase::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		auto Picker = Cast<IAbilityPickerInterface>(OtherActor);
		if (Picker)
		{
			OnPickUp(Picker);
		}
	}
}

void AAbilityPickUpBase::OnPickUp(IAbilityPickerInterface* InPicker)
{
	if(InPicker)
	{
		InPicker->PickUp(this);
	}
	UObjectPoolModuleBPLibrary::DespawnObject(this);
}

void AAbilityPickUpBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilityPickUpBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();
	Item = FAbilityItem::Empty;
}

void AAbilityPickUpBase::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	const auto SaveData = InSaveData->CastRef<FPickUpSaveData>();
	Item = SaveData.Item;
	SetActorLocation(SaveData.Location);
}

FSaveData* AAbilityPickUpBase::ToData()
{
	static auto saveData = FPickUpSaveData();

	saveData.Item = Item;
	saveData.Location = GetActorLocation();

	return &saveData;
}
