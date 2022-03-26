// Fill out your copyright notice in the Description page of Project Settings.


#include "PickUp/PickUp.h"
#include "Gameplay/DWGameMode.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "World/Chunk.h"
#include "Inventory/Inventory.h"
#include "Character/AbilityCharacterBase.h"
#include "Inventory/CharacterInventory.h"

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
	OwnerChunk = nullptr;
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

void APickUp::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		auto character = Cast<AAbilityCharacterBase>(OtherActor);
		if (character)
		{
			OnPickUp(character);
		}
	}
}

void APickUp::OnPickUp(AAbilityCharacterBase* InPicker)
{
	if(!OwnerChunk || !OwnerChunk->IsValidLowLevel()) return;

	OwnerChunk->DestroyPickUp(this);
	if(InPicker && InPicker->GetInventory())
	{
		InPicker->GetInventory()->AdditionItemByRange(Item, -1);
	}
}

void APickUp::LoadData(FPickUpSaveData InPickUpData)
{
	if (!InPickUpData.bSaved) return;

	SetActorLocation(InPickUpData.Location);
	Item = InPickUpData.Item;
}

FPickUpSaveData APickUp::ToData(bool bSaved) const
{
	auto data = FPickUpSaveData();

	data.bSaved = bSaved;

	data.Item = Item;
	data.Location = GetActorLocation();

	return data;
}

// Called every frame
void APickUp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
