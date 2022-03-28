// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Object/PickUp/PickUpVoxel.h"

#include "Components/BoxComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/VoxelAssetBase.h"

// Sets default values
APickUpVoxel::APickUpVoxel()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void APickUpVoxel::BeginPlay()
{
	Super::BeginPlay();

}

void APickUpVoxel::Initialize(FItem InItem, bool bPreview /*= false*/)
{
	Super::Initialize(InItem, bPreview);
	BoxComponent->SetBoxExtent(Item.GetData<UVoxelAssetBase>()->Range * AVoxelModule::GetWorldData()->BlockSize * (1 / Item.GetData<UVoxelAssetBase>()->Range.Z) * 0.2f);
	if(UVoxelMeshComponent* VoxelMeshComponent = Cast<UVoxelMeshComponent>(MeshComponent))
	{
		VoxelMeshComponent->Initialize(!bPreview ? EVoxelMeshType::PickUp : EVoxelMeshType::PreviewItem);
		VoxelMeshComponent->BuildVoxel(FVoxelItem(Item.ID));
		VoxelMeshComponent->CreateMesh(0, false);
	}
}

void APickUpVoxel::OnPickUp(IPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}
