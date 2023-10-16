// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntity.h"

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
	MeshComponent->Initialize(EVoxelMeshNature::Entity);

	VoxelID = FPrimaryAssetId();
	Auxiliary = nullptr;
}

void AVoxelEntity::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AVoxelEntity::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	VoxelID = FPrimaryAssetId();
	MeshComponent->ClearMesh();
	DestroyAuxiliary();
}

void AVoxelEntity::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelItem>();

	if(VoxelID == SaveData.ID) return;

	VoxelID = SaveData.ID;
	
	MeshComponent->CreateVoxel(SaveData);

	if(VoxelID.IsValid())
	{
		const UVoxelData& VoxelData = SaveData.GetVoxelData();
		if(VoxelData.AuxiliaryClass)
		{
			if(Auxiliary && Auxiliary->GetClass() != VoxelData.AuxiliaryClass)
			{
				DestroyAuxiliary();
			}
			if(!Auxiliary)
			{
				Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, VoxelData.AuxiliaryClass);
				Auxiliary->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
				Auxiliary->Execute_SetActorVisible(Auxiliary, Execute_IsVisible(this));
			}
			if(Auxiliary)
			{
				Auxiliary->LoadSaveData(new FVoxelAuxiliarySaveData(SaveData, EVoxelMeshNature::Entity));
			}
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

FSaveData* AVoxelEntity::ToData()
{
	return nullptr;
}

void AVoxelEntity::DestroyAuxiliary()
{
	if(!Auxiliary) return;
	
	Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	UObjectPoolModuleBPLibrary::DespawnObject(Auxiliary);
	Auxiliary = nullptr;
}
