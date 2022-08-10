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
	
	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(FName("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	VoxelID = FPrimaryAssetId();
}

// Called when the game starts or when spawned
void AVoxelEntity::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelEntity::Initialize(FPrimaryAssetId InVoxelID)
{
	VoxelID = InVoxelID;
	if(GetVoxelData().IsValid())
	{
		MeshComponent->Initialize(EVoxelMeshNature::Entity);
		MeshComponent->BuildVoxel(FVoxelItem(VoxelID));
		MeshComponent->CreateMesh(0, false);
	}
}

void AVoxelEntity::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AVoxelEntity::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();
	
	VoxelID = FPrimaryAssetId();
	MeshComponent->ClearMesh();
}

UVoxelData& AVoxelEntity::GetVoxelData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(VoxelID);
}
