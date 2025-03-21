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

	VoxelItem = FPrimaryAssetId();
	VoxelScope = EVoxelScope::Entity;
	VoxelItem.Auxiliary = nullptr;
}

void AVoxelEntity::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AVoxelEntity::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	VoxelItem = FPrimaryAssetId();
	MeshComponent->ClearMesh();
	DestroyAuxiliary();
}

void AVoxelEntity::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	MeshComponent->Initialize(VoxelScope);
}

void AVoxelEntity::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelItem>();

	VoxelItem = SaveData;
	
	if(VoxelItem.IsValid())
	{
		const UVoxelData& VoxelData = SaveData.GetVoxelData();
		if(VoxelData.AuxiliaryClass)
		{
			if(VoxelItem.Auxiliary && VoxelItem.Auxiliary->GetClass() != VoxelData.AuxiliaryClass)
			{
				DestroyAuxiliary();
			}
			SpawnAuxiliary();
		}
		else if(VoxelItem.Auxiliary)
		{
			DestroyAuxiliary();
		}
	}
	else if(VoxelItem.Auxiliary)
	{
		DestroyAuxiliary();
	}

	MeshComponent->CreateVoxel(SaveData);
}

FSaveData* AVoxelEntity::ToData()
{
	return nullptr;
}

void AVoxelEntity::SpawnAuxiliary()
{
	if(VoxelItem.IsValid() && !VoxelItem.Auxiliary)
	{
		const UVoxelData& VoxelData = VoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(nullptr, nullptr, VoxelData.AuxiliaryClass))
			{
				Auxiliary->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
				Auxiliary->Execute_SetActorVisible(Auxiliary, Execute_IsVisible(this));
				Auxiliary->LoadSaveData(new FVoxelAuxiliarySaveData(VoxelItem, VoxelScope));
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
