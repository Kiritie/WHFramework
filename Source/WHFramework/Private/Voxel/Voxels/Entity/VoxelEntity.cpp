// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntity.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

// Sets default values
AVoxelEntity::AVoxelEntity()
{
	PrimaryActorTick.bCanEverTick = false;
	
	MeshComponent = CreateDefaultSubobject<UVoxelMeshComponent>(FName("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	VoxelItem = FVoxelItem::Empty;
	VoxelScope = EVoxelScope::Entity;
}

void AVoxelEntity::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void AVoxelEntity::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	
	DestroyAuxiliary();
	MeshComponent->ClearMesh();

	VoxelItem = FVoxelItem::Empty;
}

void AVoxelEntity::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	MeshComponent->Initialize(VoxelScope);
}

void AVoxelEntity::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData.GetRef<FVoxelItem>();

	DestroyAuxiliary();

	VoxelItem = SaveData;

	if(VoxelItem.IsValid())
	{
		const UVoxelData& VoxelData = VoxelItem.GetData();
		if(VoxelData.AuxiliaryClass)
		{
			SpawnAuxiliary();
		}
		else
		{
			DestroyAuxiliary();
		}
	}
	else
	{
		DestroyAuxiliary();
	}

	MeshComponent->CreateVoxel(VoxelItem);
}

FParameter AVoxelEntity::ToData()
{
	return FParameter();
}

void AVoxelEntity::SpawnAuxiliary()
{
	if(VoxelItem.IsValid() && !VoxelItem.Auxiliary)
	{
		const UVoxelData& VoxelData = VoxelItem.GetData();
		if(VoxelData.AuxiliaryClass)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(VoxelData.AuxiliaryClass))
			{
				Auxiliary->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
				Auxiliary->Execute_SetActorVisible(Auxiliary, Execute_IsVisible(this));
				auto SaveData = FVoxelAuxiliarySaveData(VoxelItem, VoxelScope);
				Auxiliary->LoadSaveData(FParameter(SaveData));
				VoxelItem.Auxiliary = Auxiliary;
			}
		}
	}
}

void AVoxelEntity::DestroyAuxiliary()
{
	if(VoxelItem.IsValid() && VoxelItem.Auxiliary)
	{
		VoxelItem.Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		UObjectPoolModuleStatics::DespawnObject(VoxelItem.Auxiliary);
		VoxelItem.Auxiliary = nullptr;
	}
}
