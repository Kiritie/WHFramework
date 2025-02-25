// Fill out your copyright notice in the Description page of Project Settings.PickUp/


#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Common/CommonStatics.h"
#include "Common/CommonTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Generators/VoxelBiomeGenerator.h"
#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "Voxel/Generators/VoxelCaveGenerator.h"
#include "Voxel/Generators/VoxelHeightGenerator.h"
#include "Voxel/Generators/VoxelHumidityGenerator.h"
#include "Voxel/Generators/VoxelRainGenerator.h"
#include "Voxel/Generators/VoxelTemperatureGenerator.h"
#include "Voxel/Generators/VoxelTerrainGenerator.h"
#include "Voxel/Generators/VoxelFoliageGenerator.h"
#include "Voxel/Generators/VoxelOreGenerator.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponents = TMap<EVoxelNature, UVoxelMeshComponent*>();

	Batch = -1;
	Index = FIndex::ZeroIndex;
	bBuilded = false;
	bGenerated = false;
	bChanged = false;
	Module = nullptr;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TMap<EDirection, AVoxelChunk*>();
	ITER_DIRECTION(Iter, Neighbors.Add(Iter); )
}

void AVoxelChunk::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void AVoxelChunk::OnDespawn_Implementation(bool bRecovery)
{
	Module->GetWorldData().SetChunkData(Index, GetSaveData<FVoxelChunkSaveData>(true));

	Index = FIndex::ZeroIndex;
	Batch = -1;
	bBuilded = false;
	bGenerated = false;
	bChanged = false;
	Module = nullptr;

	DestroyMeshComponents();

	DestroySceneActors();

	BreakNeighbors();

	ITER_MAP(VoxelMap, Iter,
		DestroyAuxiliary(Iter.Value);
	)
	VoxelMap.Empty();

	TopographyMap.Empty();

	BuildingDatas.Empty();

	SetActorLocationAndRotation(UNDER_Vector, FRotator::ZeroRotator);

	Execute_SetActorVisible(this, false);
}

void AVoxelChunk::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	
	TArray<FString> VoxelDatas;
	SaveData.VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
	for(auto& Iter : VoxelDatas)
	{
		FVoxelItem VoxelItem = FVoxelItem(Iter);
		SetVoxel(VoxelItem.Index, VoxelItem);
	}
	
	TArray<FString> TopographyDatas;
	SaveData.TopographyDatas.ParseIntoArray(TopographyDatas, TEXT("|"));
	for(auto& Iter : TopographyDatas)
	{
		FVoxelTopography Topography = FVoxelTopography(Iter);
		SetTopography(Topography.Index, Topography);
	}

	BuildingDatas = SaveData.BuildingDatas;
	for(auto& Iter : BuildingDatas)
	{
		AddBuilding(Iter);
	}

	for(auto& Iter : SaveData.AuxiliaryDatas)
	{
		FVoxelItem& VoxelItem = GetVoxel(Iter.VoxelItem.Index);
		VoxelItem.AuxiliaryData = &Iter;
	}
	bBuilded = true;
}

FSaveData* AVoxelChunk::ToData()
{
	static FVoxelChunkSaveData SaveData;
	SaveData = FVoxelChunkSaveData();

	SaveData.Index = Index;
	SaveData.bGenerated = bGenerated;

	ITER_MAP(VoxelMap, Iter,
		if(Iter.Value.IsValid())
		{
			if(bChanged)
			{
				SaveData.VoxelDatas.Appendf(TEXT("%s|"), *Iter.Value.ToSaveData(true));
			}
			if(Iter.Value.Auxiliary)
			{
				SaveData.AuxiliaryDatas.Add(Iter.Value.Auxiliary->GetSaveDataRef<FVoxelAuxiliarySaveData>(true));
			}
		}
	)
	if(bChanged)
	{
		SaveData.VoxelDatas.RemoveFromEnd(TEXT("|"));
	}
	else if(Module->GetWorldData().IsExistChunkData(Index))
	{
		SaveData.VoxelDatas = Module->GetWorldData().GetChunkData(Index)->VoxelDatas;
	}

	for(auto& Iter : TopographyMap)
	{
		SaveData.TopographyDatas.Appendf(TEXT("%s|"), *Iter.Value.ToSaveData());
	}
	SaveData.TopographyDatas.RemoveFromEnd(TEXT("|"));

	SaveData.BuildingDatas = BuildingDatas;

	for(auto& Iter : SceneActorMap)
	{
		if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(Iter.Value))
		{
			SaveData.PickUpDatas.Add(PickUp->GetSaveDataRef<FPickUpSaveData>(true));
		}
	}

	return &SaveData;
}

void AVoxelChunk::Initialize(UVoxelModule* InModule, FIndex InIndex, int32 InBatch)
{
	Module = InModule;
	Index = InIndex;
	Batch = InBatch;
	if(Index.Z == 0)
	{
		ITER_INDEX2D(VoxelIndex, Module->GetWorldData().ChunkSize, false,
			FVoxelTopography Topography;
			Topography.Index = VoxelIndex;
			TopographyMap.Add(VoxelIndex, Topography);
		)
	}
	UpdateNeighbors();
}

void AVoxelChunk::Generate(EPhase InPhase)
{
	if(!Execute_IsVisible(this))
	{
		Execute_SetActorVisible(this, true);
	}
	if(PHASEC(InPhase, EPhase::Primary))
	{
		CreateMesh();
		ITER_MAP(VoxelMap, Iter,
			if(Iter.Value.IsValid())
			{
				Iter.Value.OnGenerate();
			}
		)
		if(!bGenerated)
		{
			bGenerated = true;
			GenerateSceneActors();
		}
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bGenerated)
		{
			SpawnMeshComponents();
			BuildMesh();
		}
		CreateMesh();
		ITER_MAP(VoxelMap, Iter,
			if(Iter.Value.IsValid())
			{
				Iter.Value.OnGenerate();
			}
		)
		if(!bGenerated)
		{
			bBuilded = true;
			bGenerated = true;
		}
	}
}

void AVoxelChunk::CreateMesh()
{
	for(auto& Iter : MeshComponents)
	{
		Iter.Value->CreateMesh();
	}
}

void AVoxelChunk::BuildMap(int32 InStage)
{
	switch (InStage)
	{
		case 1:
		{
			Module->GenerateVoxel<UVoxelTemperatureGenerator>(this);
			Module->GenerateVoxel<UVoxelHumidityGenerator>(this);
			Module->GenerateVoxel<UVoxelBiomeGenerator>(this);
			Module->GenerateVoxel<UVoxelHeightGenerator>(this);
			Module->GenerateVoxel<UVoxelCaveGenerator>(this);
			Module->GenerateVoxel<UVoxelOreGenerator>(this);
			Module->GenerateVoxel<UVoxelTerrainGenerator>(this);
			Module->GenerateVoxel<UVoxelRainGenerator>(this);
			Module->GenerateVoxel<UVoxelFoliageGenerator>(this);
			break;
		}
		case 2:
		{
			Module->GenerateVoxel<UVoxelBuildingGenerator>(this);
			bBuilded = true;
			break;
		}
		default: break;
	}
}

void AVoxelChunk::BuildMesh()
{
	ITER_MAP(TMap(VoxelMap), Iter,
		const FVoxelItem& VoxelItem = Iter.Value;
		if(VoxelItem.IsValid())
		{
			GetMeshComponent(VoxelItem.GetVoxelData().Nature)->BuildVoxel(VoxelItem);
		}
	);
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex, EPhase InPhase)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InIndex.Z, InPhase);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, int32 InZ, EPhase InPhase)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InX >= WorldData.ChunkSize.X - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			Chunk->Generate(InPhase);
		}
	}
	if(InY <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			Chunk->Generate(InPhase);
		}
	}
	if(InZ <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			Chunk->Generate(InPhase);
		}
	}
}

void AVoxelChunk::UpdateNeighbors()
{
	ITER_DIRECTION(Direction,
		Neighbors[Direction] = Module->GetChunkByIndex(Index + UMathStatics::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathStatics::InvertDirection(Direction)] = this;
		}
	)
}

void AVoxelChunk::BreakNeighbors()
{
	ITER_DIRECTION(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathStatics::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	const auto& WorldData = Module->GetWorldData();
	return InIndex.X >= 0 && InIndex.X < WorldData.ChunkSize.X &&
		InIndex.Y >= 0 && InIndex.Y < WorldData.ChunkSize.Y &&
		InIndex.Z >= 0 && InIndex.Z < WorldData.ChunkSize.Z;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	const auto& WorldData = Module->GetWorldData();
	return InLocation.X >= GetWorldLocation().X && InLocation.X < GetWorldLocation().X + WorldData.GetChunkRealSize().X &&
		InLocation.Y >= GetWorldLocation().Y && InLocation.Y < GetWorldLocation().Y + WorldData.GetChunkRealSize().Y &&
		InLocation.Z >= GetWorldLocation().Z && InLocation.Z < GetWorldLocation().Z + WorldData.GetChunkRealSize().Z;
}

FIndex AVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	InLocation = (InLocation - (bWorldSpace ? GetWorldLocation() : FVector::ZeroVector)) / Module->GetWorldData().BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), FMath::FloorToInt(InLocation.Z));
}

FVector AVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	return InIndex.ToVector() * Module->GetWorldData().BlockSize + (bWorldSpace ? GetWorldLocation() : FVector::ZeroVector);
}

FIndex AVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * Module->GetWorldData().ChunkSize;
}

FIndex AVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * Module->GetWorldData().ChunkSize;
}

bool AVoxelChunk::LocalIndexToNeighbor(FIndex InIndex, EDirection& OutDirection) const
{
	const auto& WorldData = Module->GetWorldData();
	if(InIndex.X < 0)
	{
		OutDirection = EDirection::Backward;
		return true;
	}
	else if(InIndex.X >= WorldData.ChunkSize.X)
	{
		OutDirection = EDirection::Forward;
		return true;
	}
	else if(InIndex.Y < 0)
	{
		OutDirection = EDirection::Left;
		return true;
	}
	else if(InIndex.Y >= WorldData.ChunkSize.Y)
	{
		OutDirection = EDirection::Right;
		return true;
	}
	else if(InIndex.Z < 0)
	{
		OutDirection = EDirection::Down;
		return true;
	}
	else if(InIndex.Z >= WorldData.ChunkSize.Z)
	{
		OutDirection = EDirection::Up;
		return true;
	}
	return false;
}

bool AVoxelChunk::HasVoxel(FIndex InIndex, bool bSafe)
{
	return VoxelMap.Contains(InIndex) && (!bSafe || VoxelMap[InIndex].IsValid());
}

bool AVoxelChunk::HasVoxel(int32 InX, int32 InY, int32 InZ, bool bSafe)
{
	return HasVoxel(FIndex(InX, InY, InZ), bSafe);
}

bool AVoxelChunk::HasVoxelComplex(FIndex InIndex, bool bSafe)
{
	return HasVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, bSafe);
}

bool AVoxelChunk::HasVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bSafe)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->HasVoxel(InX + WorldData.ChunkSize.X, InY, InZ, bSafe);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->HasVoxel(InX - WorldData.ChunkSize.X, InY, InZ, bSafe);
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->HasVoxel(InX, InY + WorldData.ChunkSize.Y, InZ, bSafe);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->HasVoxel(InX, InY - WorldData.ChunkSize.Y, InZ, bSafe);
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			return Chunk->HasVoxel(InX, InY, InZ + WorldData.ChunkSize.Z, bSafe);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			return Chunk->HasVoxel(InX, InY, InZ - WorldData.ChunkSize.Z, bSafe);
		}
	}
	else
	{
		return HasVoxel(InX, InY, InZ, bSafe);
	}
	return false;
}

FVoxelItem& AVoxelChunk::GetVoxel(FIndex InIndex, bool bMainPart)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& AVoxelChunk::GetVoxel(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		if(bMainPart) return VoxelMap[VoxelIndex].GetMain();
		return VoxelMap[VoxelIndex];
	}
	return FVoxelItem::Empty;
}

FVoxelItem& AVoxelChunk::GetVoxelComplex(FIndex InIndex, bool bMainPart)
{
	return GetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& AVoxelChunk::GetVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->GetVoxel(InX + WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->GetVoxel(InX - WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->GetVoxel(InX, InY + WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->GetVoxel(InX, InY - WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			return Chunk->GetVoxel(InX, InY, InZ + WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z > 0)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			return Chunk->GetVoxel(InX, InY, InZ - WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z < WorldData.WorldSize.Z)
		{
			return FVoxelItem::Unknown;
		}
	}
	else
	{
		return GetVoxel(InX, InY, InZ);
	}
	return FVoxelItem::Empty;
}

bool AVoxelChunk::CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange/* = FVector::OneVector*/)
{
	ITER_INDEX(Iter, InRange, false,
		if(!GetVoxelComplex(InIndex + Iter).IsReplaceable(InVoxelItem))
		{
			return true;
		}
	)
	return false;
}

bool AVoxelChunk::CheckVoxelAdjacent(FIndex InIndex, EDirection InDirection)
{
	return CheckVoxelAdjacent(GetVoxel(InIndex), InDirection);
}

bool AVoxelChunk::CheckVoxelAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection)
{
	const FIndex AdjacentIndex = UMathStatics::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Angle);
	
	if(!InVoxelItem.IsValid() || LocalIndexToWorld(AdjacentIndex).Z <= 0) return true;
	
	const FVoxelItem& AdjacentItem = GetVoxelComplex(AdjacentIndex);
	if(AdjacentItem.IsValid())
	{
		const UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
		const UVoxelData& AdjacentData = AdjacentItem.GetVoxelData();
		switch(VoxelData.GetTransparency())
		{
			case EVoxelTransparency::Solid:
			{
				switch(AdjacentData.GetTransparency())
				{
					case EVoxelTransparency::Solid:
					{
						return true;
					}
					default: break;
				}
				break;
			}
			case EVoxelTransparency::Semi:
			{
				switch(AdjacentData.GetTransparency())
				{
					case EVoxelTransparency::Semi:
					{
						if(VoxelData.VoxelType == AdjacentData.VoxelType)
						{
							switch(VoxelData.VoxelType)
							{
								case EVoxelType::Oak_Leaves:
								case EVoxelType::Birch_Leaves:
								case EVoxelType::Ice:
								case EVoxelType::Glass:
								{
									return true;
								}
								default: break;
							}
						}
						break;
					}
					default: break;
				}
				break;
			}
			case EVoxelTransparency::Trans:
			{
				switch(AdjacentData.GetTransparency())
				{
					case EVoxelTransparency::Solid:
					case EVoxelTransparency::Semi:
					{
						return true;
					}
					case EVoxelTransparency::Trans:
					{
						if(VoxelData.VoxelType == AdjacentData.VoxelType)
						{
							switch(VoxelData.VoxelType)
							{
								case EVoxelType::Water:
								{
									return true;
								}
								default: break;
							}
						}
						break;
					}
					default: break;
				}
				break;
			}
			default: break;
		}
	}
	else if(AdjacentItem.IsUnknown())
	{
		return true;
	}
	return false;
}

bool AVoxelChunk::CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange, bool bFromCenter, bool bIgnoreBottom, bool bOnTheChunk)
{
	ITER_INDEX(Iter1, InRange, bFromCenter,
		ITER_DIRECTION(Iter2, 
			if(!bIgnoreBottom || Iter2 != EDirection::Down)
			{
				const FIndex NeighborIndex = InIndex + Iter1 + UMathStatics::DirectionToIndex(Iter2);
				if(!bOnTheChunk || IsOnTheChunk(NeighborIndex))
				{
					FVoxelItem& NeighborItem = GetVoxelComplex(NeighborIndex);
					if(NeighborItem.IsValid() && NeighborItem.GetVoxelType() == InVoxelType)
					{
						return true;
					}
				}
			}
		)
	)
	return false;
}

void AVoxelChunk::SetVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe)
{
	if(!bSafe || InVoxelItem.IsValid())
	{
		FVoxelItem VoxelItem = InVoxelItem;
		VoxelItem.Owner = this;
		VoxelItem.Index = InIndex;
		VoxelMap.Emplace(InIndex, VoxelItem);
	}
	else if(HasVoxel(InIndex))
	{
		VoxelMap.Remove(InIndex);
	}
}

void AVoxelChunk::SetVoxel(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bSafe)
{
	return SetVoxel(FIndex(InX, InY, InZ), InVoxelItem, bSafe);
}

bool AVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = false;
	if(InVoxelItem.IsValid())
	{
		if(IsOnTheChunk(InIndex))
		{
			SetVoxel(InIndex, InVoxelItem);
			if(bGenerate) VoxelMap[InIndex].OnGenerate(InAgent);
			bSuccess = true;
		}
	}
	else
	{
		if(HasVoxel(InIndex))
		{
			FVoxelItem VoxelItem = VoxelMap[InIndex];
			VoxelMap.Remove(InIndex);
			if(bGenerate) VoxelItem.OnDestroy(InAgent);
			bSuccess = true;
		}
	}
	if(bSuccess)
	{
		if(bGenerate)
		{
			Generate(EPhase::Lesser);
			GenerateNeighbors(InIndex, EPhase::Lesser);
		}
		if(InAgent)
		{
			bChanged = true;
		}
	}
	return bSuccess;
}

bool AVoxelChunk::SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelSample(FIndex(InX, InY, InZ), InVoxelItem, bGenerate, InAgent);
}

bool AVoxelChunk::SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, InVoxelItem, bGenerate, InAgent);
}

bool AVoxelChunk::SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->SetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->SetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->SetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->SetVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			return Chunk->SetVoxelComplex(InX, InY, InZ + WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Up, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX, InY, InZ - WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		const FVoxelItem VoxelItem = InVoxelItem.IsValid() ? InVoxelItem : GetVoxel(VoxelIndex);
		if(VoxelItem.IsValid())
		{
			UVoxelData& VoxelData = VoxelItem.GetVoxelData(false);
			const FVector VoxelRange = VoxelData.GetRange(VoxelItem.Angle);
			if(!CheckVoxel(VoxelIndex, InVoxelItem, VoxelRange))
			{
				if(VoxelData.bMainPart && VoxelRange != FVector::OneVector)
				{
					TMap<FIndex, FVoxelItem> VoxelItems;
					VoxelItems.Emplace(VoxelIndex, InVoxelItem.IsValid() ? InVoxelItem.ReplaceID(VoxelData.GetPrimaryAssetId()) : FVoxelItem::Empty);
					ITER_INDEX(PartIndex, VoxelRange, false,
						UVoxelData& PartData = VoxelData.GetPartData(PartIndex);
						if(!PartData.bMainPart)
						{
							VoxelItems.Emplace(VoxelIndex + PartIndex, InVoxelItem.IsValid() ? InVoxelItem.ReplaceID(PartData.GetPrimaryAssetId()) : FVoxelItem::Empty);
						}
					)
					return SetVoxelComplex(VoxelItems, bGenerate, true, InAgent);
				}
				else
				{
					return SetVoxelSample(VoxelIndex, InVoxelItem, bGenerate, InAgent);
				}
			}
		}
	}
	return false;
}

bool AVoxelChunk::SetVoxelComplex(const TMap<FIndex, FVoxelItem>& InVoxelItems, bool bGenerate, bool bFirstSample, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = true;
	TArray<EDirection> neighbors;
	for(auto& Iter : InVoxelItems)
	{
		FVoxelItem VoxelItem = GetVoxelComplex(Iter.Key);
		if(bFirstSample ? !SetVoxelSample(Iter.Key, Iter.Value, false, InAgent) :
			!SetVoxelComplex(Iter.Key, Iter.Value, false, InAgent))
		{
			bSuccess = false;
		}
		else if(bGenerate)
		{
			if(Iter.Value.IsValid())
			{
				GetVoxelComplex(Iter.Key).OnGenerate(InAgent);
			}
			else
			{
				VoxelItem.OnDestroy(InAgent);
			}
			EDirection neighbor;
			if(LocalIndexToNeighbor(Iter.Key, neighbor))
			{
				neighbors.AddUnique(neighbor);
			}
		}
		bFirstSample = false;
	}
	if(bGenerate)
	{
		Generate(EPhase::Lesser);
		for(const auto& Iter : neighbors)
		{
			if(GetNeighbor(Iter))
			{
				GetNeighbor(Iter)->Generate(EPhase::Lesser);
			}
		}
	}
	return bSuccess;
}

FVoxelTopography& AVoxelChunk::GetTopography(FIndex InIndex)
{
	if(Index.Z == 0)
	{
		if(TopographyMap.Contains(InIndex))
		{
			return TopographyMap[InIndex];
		}
		return *new FVoxelTopography();
	}
	return Module->GetChunkByIndex(FIndex(Index.X, Index.Y, 0))->GetTopography(InIndex);
}

FVoxelTopography& AVoxelChunk::GetTopography(int32 InX, int32 InY, int32 InZ)
{
	return GetTopography(FIndex(InX, InY, InZ));
}

void AVoxelChunk::SetTopography(FIndex InIndex, const FVoxelTopography& InTopography)
{
	if(Index.Z == 0)
	{
		TopographyMap.Emplace(InIndex, InTopography);
		return;
	}
	Module->GetChunkByIndex(FIndex(Index.X, Index.Y, 0))->SetTopography(InIndex, InTopography);
}

void AVoxelChunk::SetTopography(int32 InX, int32 InY, int32 InZ, const FVoxelTopography& InTopography)
{
	SetTopography(FIndex(InX, InY, InZ), InTopography);
}

void AVoxelChunk::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
}

bool AVoxelChunk::HasSceneActor(const FString& InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(FGuid(InID))) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No SceneActor, Actor name: %s"), *InID), EDC_Voxel, EDV_Error);
	return false;
}

AActor* AVoxelChunk::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActor(InID, bEnsured))
	{
		return SceneActorMap[FGuid(InID)];
	}
	return nullptr;
}

bool AVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(!SceneActorMap.Contains(Execute_GetActorID(InActor)))
	{
		SceneActorMap.Add(Execute_GetActorID(InActor), InActor);
		if(Execute_GetContainer(InActor) != this)
		{
			if(Execute_GetContainer(InActor))
			{
				Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
			}
			Execute_SetContainer(InActor, this);
		}
		return true;
	}
	return false;
}

bool AVoxelChunk::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(SceneActorMap.Contains(Execute_GetActorID(InActor)))
	{
		SceneActorMap.Remove(Execute_GetActorID(InActor));
		if(Execute_GetContainer(InActor) == this)
		{
			Execute_SetContainer(InActor, nullptr);
		}
		return true;
	}
	return false;
}

void AVoxelChunk::GenerateSceneActors()
{
	auto& WorldData = Module->GetWorldData();
	
	if(WorldData.IsChunkGenerated(Index))
	{
		LoadSceneActors(WorldData.GetChunkData(Index));
	}
	else if(Index.Z == WorldData.WorldSize.Z - 1)
	{
		SpawnSceneActors();
	}

	GenerateBuildings();
}

void AVoxelChunk::LoadSceneActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();

	for(auto& Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleStatics::SpawnAbilityPickUp(&Iter, this);
	}

	for(auto& Iter : SaveData.BuildingDatas)
	{
		SpawnBuilding(Iter);
	}
}

void AVoxelChunk::SpawnSceneActors()
{
	
}

void AVoxelChunk::DestroySceneActors()
{
	for(auto& Iter : TMap(SceneActorMap))
	{
		if(UCommonStatics::GetPlayerPawn() != Iter.Value)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		}
		SceneActorMap.Remove(Iter.Key);
	}
	
	DestroyBuildings();
}

void AVoxelChunk::GenerateBuildings()
{
	for(auto& Iter : BuildingDatas)
	{
		SpawnBuilding(Iter);
	}
}

void AVoxelChunk::SpawnBuilding(FVoxelBuildingSaveData& InBuildingData)
{
	if(TSubclassOf<AActor> BuildingClass = GetBuildingClassByID(InBuildingData.ID))
	{
		if(AActor* SpawnActor = UObjectPoolModuleStatics::SpawnObject<AActor>(nullptr, nullptr, BuildingClass))
		{
			SpawnActor->SetActorScale3D(FVector::OneVector * (Module->GetWorldData().BlockSize / 100.f));
			SpawnActor->SetActorLocationAndRotation(InBuildingData.Location * Module->GetWorldData().BlockSize, FRotator(0, InBuildingData.Angle * 90.f, 0));
			InBuildingData.Actor = SpawnActor;
		}
	}
}

void AVoxelChunk::DestroyBuilding(FVoxelBuildingSaveData& InBuildingData)
{
	if(InBuildingData.Actor)
	{
		UObjectPoolModuleStatics::DespawnObject(InBuildingData.Actor);
		InBuildingData.Actor = nullptr;
	}
}

void AVoxelChunk::DestroyBuildings()
{
	for(auto& Iter : BuildingDatas)
	{
		DestroyBuilding(Iter);
	}
}

void AVoxelChunk::AddBuilding(const FVoxelBuildingSaveData& InBuildingData)
{
	BuildingDatas.Add(InBuildingData);
}

TSubclassOf<AActor> AVoxelChunk::GetBuildingClassByID(int32 InID) const
{
	return nullptr;
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const auto& VoxelData = InVoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass && VoxelData.bMainPart)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(nullptr, nullptr, VoxelData.AuxiliaryClass))
			{
				FVoxelAuxiliarySaveData AuxiliaryData;
				if(InVoxelItem.AuxiliaryData)
				{
					AuxiliaryData = InVoxelItem.AuxiliaryData->CastRef<FVoxelAuxiliarySaveData>();
				}
				AuxiliaryData.VoxelItem = InVoxelItem;
				Auxiliary->LoadSaveData(&AuxiliaryData);
				InVoxelItem.Auxiliary = Auxiliary;
				AddSceneActor(Auxiliary);
				return Auxiliary;
			}
		}
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && InVoxelItem.Auxiliary)
	{
		UObjectPoolModuleStatics::DespawnObject(InVoxelItem.Auxiliary);
		InVoxelItem.Auxiliary = nullptr;
	}
}

void AVoxelChunk::SpawnMeshComponents(int32 InStage)
{
	if(InStage & 1)
	{
		TArray<EVoxelNature> _MeshVoxelNatures;
		ITER_MAP(TMap(VoxelMap), Iter,
			const FVoxelItem& VoxelItem = Iter.Value;
			if(VoxelItem.IsValid())
			{
				const UVoxelData& VoxelData = VoxelItem.GetVoxelData();
				if(!_MeshVoxelNatures.Contains(VoxelData.Nature))
				{
					_MeshVoxelNatures.Add(VoxelData.Nature);
				}
			}
		)
		MeshVoxelNatures = _MeshVoxelNatures;
	}
	if(InStage & 2)
	{
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
				MeshComponent->Initialize(EVoxelScope::Chunk, Iter);
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
}

void AVoxelChunk::DestroyMeshComponents()
{
	for(auto& Iter : MeshComponents)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Value);
	}
	MeshComponents.Empty();
}

UVoxelMeshComponent* AVoxelChunk::GetMeshComponent(EVoxelNature InVoxelNature)
{
	return MeshComponents[InVoxelNature];
}

AVoxelChunk* AVoxelChunk::GetOrSpawnNeighbor(EDirection InDirection, bool bAddToQueue)
{
	return Neighbors[InDirection] ? Neighbors[InDirection] : Module->SpawnChunk(Index + UMathStatics::DirectionToIndex(InDirection), bAddToQueue);
}

FIndex AVoxelChunk::GetWorldIndex() const
{
	return Index * Module->GetWorldData().ChunkSize;
}

FVector AVoxelChunk::GetWorldLocation() const
{
	return UVoxelModuleStatics::ChunkIndexToLocation(Index);
}
