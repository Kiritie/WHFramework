// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpBase.h"

#include "Ability/PickUp/AbilityPickerInterface.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

// Sets default values
AAbilityPickUpBase::AAbilityPickUpBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetCollisionProfileName(TEXT("PickUp"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityPickUpBase::OnOverlap);
	SetRootComponent(BoxComponent);

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent"));
	RotatingComponent->RotationRate = FRotator(0, 180, 0);

	MeshComponent = nullptr;

	Item = FAbilityItem::Empty;
	Container = nullptr;
}

// Called when the game starts or when spawned
void AAbilityPickUpBase::BeginPlay()
{
	Super::BeginPlay();

}

void AAbilityPickUpBase::Initialize(FAbilityItem InItem)
{
	Item = InItem;
}

void AAbilityPickUpBase::RemoveFromContainer_Implementation()
{
	if(Container)
	{
		Cast<ISceneContainerInterface>(Container)->RemoveSceneActor(this);
		Container= nullptr;
	}
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
	Execute_RemoveFromContainer(this);
}

void AAbilityPickUpBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{

}

void AAbilityPickUpBase::OnDespawn_Implementation()
{
	Item = FAbilityItem::Empty;
	Container = nullptr;
}

void AAbilityPickUpBase::LoadData(FSaveData* InSaveData)
{
	FPickUpSaveData SaveData = InSaveData->ToRef<FPickUpSaveData>();
	if (!SaveData.bSaved) return;

	SetActorLocation(SaveData.Location);
	Item = SaveData.Item;
}

FSaveData* AAbilityPickUpBase::ToData()
{
	static auto saveData = FPickUpSaveData();

	saveData.Item = Item;
	saveData.Location = GetActorLocation();

	return &saveData;
}
