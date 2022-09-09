// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntity.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

// Sets default values
AVoxelEntity::AVoxelEntity()
{
	PrimaryActorTick.bCanEverTick = false;
	
	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(FName("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->Initialize(EVoxelMeshNature::Entity);

	VoxelID = FPrimaryAssetId();
	Auxiliary = nullptr;
}

// Called when the game starts or when spawned
void AVoxelEntity::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelEntity::Initialize(FPrimaryAssetId InVoxelID)
{
	VoxelID = InVoxelID;
	
	MeshComponent->CreateVoxel(InVoxelID);

	if(InVoxelID.IsValid())
	{
		Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(InVoxelID).AuxiliaryClass);
		if(Auxiliary)
		{
			Auxiliary->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Auxiliary->Initialize(InVoxelID);
		}
	}
	else if(Auxiliary)
	{
		Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		UObjectPoolModuleBPLibrary::DespawnObject(Auxiliary);
		Auxiliary = nullptr;
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
	if(Auxiliary)
	{
		Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		UObjectPoolModuleBPLibrary::DespawnObject(Auxiliary);
		Auxiliary = nullptr;
	}
}
