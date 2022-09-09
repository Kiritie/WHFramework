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
	MeshComponent->SetRelativeScale3D(FVector(0.3f));
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
		const UVoxelData& voxelData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
		if(voxelData.AuxiliaryClass)
		{
			if(Auxiliary && !Auxiliary->IsA(voxelData.AuxiliaryClass))
			{
				DestroyAuxiliary();
			}
			if(!Auxiliary)
			{
				Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, voxelData.AuxiliaryClass);
				Auxiliary->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
				Auxiliary->Execute_SetActorVisible(Auxiliary, Execute_IsVisible(this));
			}
			Auxiliary->Initialize(InVoxelID);
		}
		else if(Auxiliary)
		{
			DestroyAuxiliary();
		}
	}
	else if(Auxiliary)
	{
		DestroyAuxiliary();
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
	DestroyAuxiliary();
}

void AVoxelEntity::DestroyAuxiliary()
{
	if(!Auxiliary) return;
	
	Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	UObjectPoolModuleBPLibrary::DespawnObject(Auxiliary);
	Auxiliary = nullptr;
}
