// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntity.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelEntity::AVoxelEntity()
{
	PrimaryActorTick.bCanEverTick = false;
	
	VoxelMesh = CreateDefaultSubobject<UVoxelMeshComponent>(FName("VoxelMesh"));
	VoxelMesh->SetupAttachment(RootComponent, FName("VoxelMesh"));
	VoxelMesh->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	VoxelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	VoxelID = FPrimaryAssetId();
}

// Called when the game starts or when spawned
void AVoxelEntity::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelEntity::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	VoxelID = InParams[0].GetPointerValueRef<FPrimaryAssetId>();
	if(GetVoxelData().IsValid())
	{
		VoxelMesh->Initialize(EVoxelMeshType::Entity);
		VoxelMesh->BuildVoxel(FVoxelItem(VoxelID));
		VoxelMesh->CreateMesh(0, false);
	}
}

void AVoxelEntity::OnDespawn_Implementation()
{
	VoxelID = FPrimaryAssetId();
	VoxelMesh->ClearMesh();
}

UVoxelData& AVoxelEntity::GetVoxelData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(VoxelID);
}
