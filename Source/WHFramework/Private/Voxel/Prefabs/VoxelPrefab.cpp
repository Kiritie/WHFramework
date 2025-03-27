// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Prefabs/VoxelPrefab.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

// Sets default values
AVoxelPrefab::AVoxelPrefab()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponents = TMap<EVoxelNature, UVoxelMeshComponent*>();
	VoxelMap = TMap<FIndex, FVoxelItem>();
}

void AVoxelPrefab::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AVoxelPrefab::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	DestroyMeshComponents();
	DestroyAuxiliarys();
	VoxelMap.Empty();
}

void AVoxelPrefab::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AVoxelPrefab::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelPrefabSaveData>();

	DestroyAuxiliarys();
	
	VoxelMap.Empty();

	TArray<FString> VoxelDatas;
	SaveData.VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
	for(auto& Iter : VoxelDatas)
	{
		FVoxelItem VoxelItem = FVoxelItem(Iter, true);
		VoxelItem.Index.Z -= 1;
		SetVoxel(VoxelItem.Index, VoxelItem);
	}

	for(auto& Iter : VoxelMap)
	{
		FVoxelItem& VoxelItem = Iter.Value;
		SpawnAuxiliary(VoxelItem);
	}

	SpawnMeshComponents();
	BuildMesh();
	CreateMesh();
}

FSaveData* AVoxelPrefab::ToData()
{
	static FVoxelPrefabSaveData SaveData;
	SaveData = FVoxelPrefabSaveData();

	ITER_MAP(VoxelMap, Iter,
		if(Iter.Value.IsValid())
		{
			SaveData.VoxelDatas.Appendf(TEXT("%s|"), *Iter.Value.ToSaveData(true, true));
		}
	)
	SaveData.VoxelDatas.RemoveFromEnd(TEXT("|"));
	return &SaveData;
}

void AVoxelPrefab::CreateMesh()
{
	for(auto& Iter : MeshComponents)
	{
		Iter.Value->CreateMesh();
	}
}

void AVoxelPrefab::BuildMesh()
{
	ITER_MAP(TMap(VoxelMap), Iter,
		const FVoxelItem& VoxelItem = Iter.Value;
		if(VoxelItem.IsValid())
		{
			GetMeshComponent(VoxelItem.GetVoxelData().Nature)->BuildVoxel(VoxelItem);
		}
	)
}

void AVoxelPrefab::SpawnMeshComponents()
{
	TArray<EVoxelNature> MeshVoxelNatures;
	ITER_MAP(TMap(VoxelMap), Iter,
		const FVoxelItem& VoxelItem = Iter.Value;
		if(VoxelItem.IsValid())
		{
			const UVoxelData& VoxelData = VoxelItem.GetVoxelData();
			if(!MeshVoxelNatures.Contains(VoxelData.Nature))
			{
				MeshVoxelNatures.Add(VoxelData.Nature);
			}
		}
	)
	TArray<EVoxelNature> _MeshVoxelNatures;
	MeshComponents.GenerateKeyArray(_MeshVoxelNatures);
	TMap<EVoxelNature, UVoxelMeshComponent*> _MeshComponents;
	for(auto& Iter : MeshVoxelNatures)
	{
		UVoxelMeshComponent* MeshComponent = nullptr;
		if(MeshComponents.Contains(Iter))
		{
			MeshComponent = MeshComponents[Iter];
		}
		else
		{
			MeshComponent = UObjectPoolModuleStatics::SpawnObject<UVoxelMeshComponent>(this);
			MeshComponent->Initialize(EVoxelScope::Prefab, Iter);
		}
		_MeshComponents.Add(Iter, MeshComponent);
		if(_MeshVoxelNatures.Contains(Iter))
		{
			_MeshVoxelNatures.Remove(Iter);
		}
	}
	MeshVoxelNatures.Empty();
	for(auto& Iter : _MeshVoxelNatures)
	{
		UObjectPoolModuleStatics::DespawnObject(MeshComponents[Iter]);
	}
	MeshComponents = _MeshComponents;
}

void AVoxelPrefab::DestroyMeshComponents()
{
	for(auto& Iter : MeshComponents)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Value);
	}
	MeshComponents.Empty();
}

AVoxelAuxiliary* AVoxelPrefab::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(nullptr, nullptr, VoxelData.AuxiliaryClass))
			{
				Auxiliary->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
				Auxiliary->Execute_SetActorVisible(Auxiliary, Execute_IsVisible(this));
				auto SaveData = FVoxelAuxiliarySaveData(InVoxelItem, EVoxelScope::Prefab);
				Auxiliary->LoadSaveData(&SaveData);
				InVoxelItem.Auxiliary = Auxiliary;
				return Auxiliary;
			}
		}
	}
	return nullptr;
}

void AVoxelPrefab::DestroyAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && InVoxelItem.Auxiliary)
	{
		UObjectPoolModuleStatics::DespawnObject(InVoxelItem.Auxiliary);
		InVoxelItem.Auxiliary = nullptr;
	}
}

void AVoxelPrefab::DestroyAuxiliarys()
{
	ITER_MAP(TMap(VoxelMap), Iter,
		FVoxelItem& VoxelItem = Iter.Value;
		DestroyAuxiliary(VoxelItem);
	)
}

bool AVoxelPrefab::HasVoxel(FIndex InIndex)
{
	return VoxelMap.Contains(InIndex);
}

bool AVoxelPrefab::HasVoxel(int32 InX, int32 InY, int32 InZ)
{
	return HasVoxel(FIndex(InX, InY, InZ));
}

FVoxelItem& AVoxelPrefab::GetVoxel(FIndex InIndex)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z);
}

FVoxelItem& AVoxelPrefab::GetVoxel(int32 InX, int32 InY, int32 InZ)
{
	if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		return VoxelMap[VoxelIndex];
	}
	return FVoxelItem::Empty;
}

void AVoxelPrefab::SetVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem)
{
	FVoxelItem VoxelItem = InVoxelItem;
	VoxelItem.Index = InIndex;
	VoxelMap.Emplace(InIndex, VoxelItem);
}

void AVoxelPrefab::SetVoxel(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem)
{
	return SetVoxel(FIndex(InX, InY, InZ), InVoxelItem);
}

UVoxelMeshComponent* AVoxelPrefab::GetMeshComponent(EVoxelNature InVoxelNature)
{
	return MeshComponents[InVoxelNature];
}
