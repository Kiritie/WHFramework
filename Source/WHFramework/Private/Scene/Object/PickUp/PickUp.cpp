// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Object/PickUp/PickUp.h"

#include "GameFramework/RotatingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Scene/Object/PickUp/PickerInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Chunks/VoxelChunk.h"

// Sets default values
APickUp::APickUp()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetCollisionProfileName(TEXT("DW_PickUp"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &APickUp::OnOverlap);
	SetRootComponent(BoxComponent);

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent"));
	RotatingComponent->RotationRate = FRotator(0, 180, 0);

	MeshComponent = nullptr;

	Item = FItem::Empty;
	Container = nullptr;
}

// Called when the game starts or when spawned
void APickUp::BeginPlay()
{
	Super::BeginPlay();

}

void APickUp::Initialize(FItem InItem, bool bPreview /*= false*/)
{
	Item = InItem;
	RotatingComponent->SetActive(!bPreview);
	BoxComponent->SetCollisionEnabled(!bPreview ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void APickUp::RemoveFromContainer()
{
	if(Container)
	{
		Container->RemoveSceneObject(this);
		Container= nullptr;
	}
}

void APickUp::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		auto Picker = Cast<IPickerInterface>(OtherActor);
		if (Picker)
		{
			OnPickUp(Picker);
		}
	}
}

void APickUp::OnPickUp(IPickerInterface* InPicker)
{
	if(InPicker)
	{
		InPicker->PickUp(this);
	}
	RemoveFromContainer();
}

void APickUp::LoadData(FSaveData* InSaveData)
{
	FPickUpSaveData SaveData = *static_cast<FPickUpSaveData*>(InSaveData);
	if (!SaveData.bSaved) return;

	SetActorLocation(SaveData.Location);
	Item = SaveData.Item;
}

FSaveData* APickUp::ToData(bool bSaved)
{
	auto data = FPickUpSaveData();

	data.bSaved = bSaved;

	data.Item = Item;
	data.Location = GetActorLocation();

	return &data;
}

// Called every frame
void APickUp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
