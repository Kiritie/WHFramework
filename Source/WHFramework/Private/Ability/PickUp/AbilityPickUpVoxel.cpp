// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpVoxel.h"

#include "Components/BoxComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AAbilityPickUpVoxel::AAbilityPickUpVoxel()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AAbilityPickUpVoxel::BeginPlay()
{
	Super::BeginPlay();

}

void AAbilityPickUpVoxel::Initialize(FAbilityItem InItem)
{
	Super::Initialize(InItem);
	BoxComponent->SetBoxExtent(Item.GetData<UVoxelData>().Range * AVoxelModule::GetWorldData()->BlockSize * (1 / Item.GetData<UVoxelData>().Range.Z) * 0.2f);
	if(UVoxelMeshComponent* VoxelMeshComponent = Cast<UVoxelMeshComponent>(MeshComponent))
	{
		VoxelMeshComponent->Initialize(EVoxelMeshType::PickUp);
		VoxelMeshComponent->BuildVoxel(FVoxelItem(Item.ID));
		VoxelMeshComponent->CreateMesh(0, false);
	}
}

void AAbilityPickUpVoxel::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}
