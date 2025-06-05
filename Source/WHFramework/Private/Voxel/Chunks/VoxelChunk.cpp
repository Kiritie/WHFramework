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
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Generators/VoxelBiomeGenerator.h"
#include "Voxel/Generators/VoxelVillageGenerator.h"
#include "Voxel/Generators/VoxelCaveGenerator.h"
#include "Voxel/Generators/VoxelHeightGenerator.h"
#include "Voxel/Generators/VoxelHumidityGenerator.h"
#include "Voxel/Generators/VoxelRainGenerator.h"
#include "Voxel/Generators/VoxelTemperatureGenerator.h"
#include "Voxel/Generators/VoxelTerrainGenerator.h"
#include "Voxel/Generators/VoxelFoliageGenerator.h"
#include "Voxel/Generators/VoxelHoleGenerator.h"
#include "Voxel/Generators/VoxelLakeGenerator.h"
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
	VoxelMaps = FVoxelMaps();
	TopographyMap = TMap<FIndex, FVoxelTopography>();
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

	DestroyMeshComponents();

	DestroySceneActors();

	BreakNeighbors();

	VoxelMaps.Iter([this](FVoxelItem& Item){
		DestroyAuxiliary(Item);
	});
	VoxelMaps.Clear();

	TopographyMap.Empty();

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

	VoxelMaps.Iter([this](FVoxelItem& Item){
		if(Item.IsValid())
		{
			if(bChanged)
			{
				SaveData.VoxelDatas.Appendf(TEXT("%s|"), *Item.ToSaveData(false, true));
			}
			if(Item.Auxiliary)
			{
				SaveData.AuxiliaryDatas.Add(Item.Auxiliary->GetSaveDataRef<FVoxelAuxiliarySaveData>(true));
			}
		}
	});
	if(bChanged)
	{
		SaveData.VoxelDatas.RemoveFromEnd(TEXT("|"));

		for(auto& Iter : TopographyMap)
		{
			SaveData.TopographyDatas.Appendf(TEXT("%s|"), *Iter.Value.ToSaveData());
		}
		SaveData.TopographyDatas.RemoveFromEnd(TEXT("|"));
	}
	else if(Module->GetWorldData().IsExistChunkData(Index))
	{
		SaveData.VoxelDatas = Module->GetWorldData().GetChunkData(Index)->VoxelDatas;
		SaveData.TopographyDatas = Module->GetWorldData().GetChunkData(Index)->TopographyDatas;
	}

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
	
	ITER_INDEX2D(VoxelIndex, Module->GetWorldData().ChunkSize, false,
		FVoxelTopography Topography;
		Topography.Index = VoxelIndex;
		TopographyMap.Add(VoxelIndex, Topography);
	)
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
		VoxelMaps.Iter([this](FVoxelItem& Item){
			if(Item.IsValid())
			{
				Item.OnGenerate();
			}
		});
		GenerateSceneActors();
		bGenerated = true;
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		SpawnMeshComponents();
		BuildMesh();
		CreateMesh();
	}
}

void AVoxelChunk::CreateMesh()
{
	for(auto& Iter : MeshComponents)
	{
		Iter.Value->CreateMesh();
	}
}

void AVoxelChunk::ClearMap(bool bGenerate)
{
	VoxelMaps.Iter([this](FVoxelItem& Item){
		DestroyAuxiliary(Item);
	});
	VoxelMaps.Clear();
	Generate(EPhase::Lesser);
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
			break;
		}
		case 2:
		{
			Module->GenerateVoxel<UVoxelLakeGenerator>(this);
			break;
		}
		case 3:
		{
			Module->GenerateVoxel<UVoxelCaveGenerator>(this);
			break;
		}
		case 4:
		{
			Module->GenerateVoxel<UVoxelHoleGenerator>(this);
			break;
		}
		case 5:
		{
			Module->GenerateVoxel<UVoxelOreGenerator>(this);
			break;
		}
		case 6:
		{
			Module->GenerateVoxel<UVoxelTerrainGenerator>(this);
			break;
		}
		case 7:
		{
			Module->GenerateVoxel<UVoxelRainGenerator>(this);
			break;
		}
		case 8:
		{
			Module->GenerateVoxel<UVoxelFoliageGenerator>(this);
			break;
		}
		case 9:
		{
			Module->GenerateVoxel<UVoxelVillageGenerator>(this);
		}
		default:
		{
			bBuilded = true;
			break;
		}
	}
}

void AVoxelChunk::BuildPrefabMap()
{
	ITER_INDEX2D(_Index, Module->GetWorldData().ChunkSize, false,
		SetVoxel(_Index, EVoxelType::Grass);
	)
	bBuilded = true;
}

void AVoxelChunk::BuildMesh()
{
	VoxelMaps.Iter([this](FVoxelItem& Item){
		if(Item.IsValid())
		{
			GetMeshComponent(Item.GetData().Nature)->BuildVoxel(Item);
		}
	}, true);
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex, EPhase InPhase)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InPhase);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, EPhase InPhase)
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
}

void AVoxelChunk::UpdateNeighbors()
{
	ITER_DIRECTION(Direction,
		Neighbors[Direction] = Module->GetChunkByIndex(Index + FMathHelper::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[FMathHelper::InvertDirection(Direction)] = this;
		}
	)
}

void AVoxelChunk::BreakNeighbors()
{
	ITER_DIRECTION(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[FMathHelper::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	const auto& WorldData = Module->GetWorldData();
	return InIndex.X >= 0 && InIndex.X < WorldData.ChunkSize.X &&
		InIndex.Y >= 0 && InIndex.Y < WorldData.ChunkSize.Y &&
		InIndex.Z >= 0 && InIndex.Z < WorldData.SkyHeight;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	const auto& WorldData = Module->GetWorldData();
	return InLocation.X >= GetWorldLocation().X && InLocation.X < GetWorldLocation().X + WorldData.GetChunkRealSize().X &&
		InLocation.Y >= GetWorldLocation().Y && InLocation.Y < GetWorldLocation().Y + WorldData.GetChunkRealSize().Y &&
		InLocation.Z >= GetWorldLocation().Z && InLocation.Z < GetWorldLocation().Z + WorldData.GetWorldRealHeight();
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
	return false;
}

bool AVoxelChunk::HasVoxel(FIndex InIndex, bool bSafe)
{
	FScopeLock ScopeLock(&CriticalSection);

	return VoxelMaps.Has(InIndex) && (!bSafe || VoxelMaps.Get(InIndex).IsValid());
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
	FScopeLock ScopeLock(&CriticalSection);

	if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		if(bMainPart) return VoxelMaps.Get(VoxelIndex).GetMain();
		return VoxelMaps.Get(VoxelIndex);
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
	const FIndex AdjacentIndex = FMathHelper::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Angle);
	
	if(!InVoxelItem.IsValid() || AdjacentIndex.Z <= 0) return true;
	
	const FVoxelItem& AdjacentItem = GetVoxelComplex(AdjacentIndex);
	if(AdjacentItem.IsValid())
	{
		const UVoxelData& VoxelData = InVoxelItem.GetData();
		const UVoxelData& AdjacentData = AdjacentItem.GetData();
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
				const FIndex NeighborIndex = InIndex + Iter1 + FMathHelper::DirectionToIndex(Iter2);
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
	FScopeLock ScopeLock(&CriticalSection);

	if(!bSafe || InVoxelItem.IsValid())
	{
		FVoxelItem VoxelItem = InVoxelItem;
		VoxelItem.Chunk = this;
		VoxelItem.Index = InIndex;
		VoxelMaps.Set(InIndex, VoxelItem);
	}
	else if(HasVoxel(InIndex))
	{
		VoxelMaps.Remove(InIndex);
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
			if(bGenerate) VoxelMaps.Get(InIndex).OnGenerate(InAgent);
			bSuccess = true;
		}
	}
	else
	{
		if(HasVoxel(InIndex))
		{
			FVoxelItem VoxelItem = VoxelMaps.Get(InIndex);
			VoxelMaps.Remove(InIndex);
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
			if(InAgent)
			{
				bChanged = true;
			}
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
	else
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		const FVoxelItem VoxelItem = InVoxelItem.IsValid() ? InVoxelItem : GetVoxel(VoxelIndex);
		if(VoxelItem.IsValid())
		{
			const UVoxelData& VoxelData = VoxelItem.GetData(false);
			const FVector VoxelRange = VoxelData.GetRange(VoxelItem.Angle);
			if(VoxelData.IsMainPart() && VoxelRange != FVector::OneVector)
			{
				FVoxelMap VoxelMap;
				VoxelMap.Map.Emplace(VoxelIndex, InVoxelItem);
				ITER_ARRAY(VoxelData.PartDatas, PartData,
					FVoxelItem PartItem = FVoxelItem::Empty;
					if(InVoxelItem.IsValid())
					{
						PartItem.ID = PartData->GetPrimaryAssetId();
						PartItem.Angle = InVoxelItem.Angle;
					}
					VoxelMap.Map.Emplace(VoxelIndex + FMathHelper::RotateIndex(PartData->PartIndex, VoxelItem.Angle), PartItem);
				)
				return SetVoxelComplex(VoxelMap, bGenerate, true, InAgent);
			}
			else
			{
				return SetVoxelSample(VoxelIndex, InVoxelItem, bGenerate, InAgent);
			}
		}
	}
	return false;
}

bool AVoxelChunk::SetVoxelComplex(const FVoxelMap& InVoxelMap, bool bGenerate, bool bFirstSample, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = true;
	TArray<EDirection> _Neighbors;
	for(auto& Iter : InVoxelMap.Map)
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
			EDirection _Neighbor;
			if(LocalIndexToNeighbor(Iter.Key, _Neighbor))
			{
				_Neighbors.AddUnique(_Neighbor);
			}
		}
		bFirstSample = false;
	}
	if(bGenerate)
	{
		Generate(EPhase::Lesser);
		for(const auto& Iter : _Neighbors)
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
	if(TopographyMap.Contains(InIndex))
	{
		return TopographyMap[InIndex];
	}
	static FVoxelTopography Temp;
	return Temp;
}

FVoxelTopography& AVoxelChunk::GetTopography(int32 InX, int32 InY, int32 InZ)
{
	return GetTopography(FIndex(InX, InY, InZ));
}

void AVoxelChunk::SetTopography(FIndex InIndex, const FVoxelTopography& InTopography)
{
	TopographyMap.Emplace(InIndex, InTopography);
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
	else
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			SpawnSceneActors();
		});
	}
}

void AVoxelChunk::LoadSceneActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();

	for(auto& Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleStatics::SpawnAbilityPickUp(&Iter, this);
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
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const auto& VoxelData = InVoxelItem.GetData();
		if(VoxelData.AuxiliaryClass && VoxelData.IsMainPart())
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(nullptr, nullptr, VoxelData.AuxiliaryClass))
			{
				FVoxelAuxiliarySaveData AuxiliaryData;
				if(InVoxelItem.AuxiliaryData)
				{
					AuxiliaryData = InVoxelItem.AuxiliaryData->CastRef<FVoxelAuxiliarySaveData>();
				}
				AuxiliaryData.VoxelScope = EVoxelScope::Chunk;
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
		VoxelMaps.Iter([this](FVoxelItem& Item){
			if(Item.IsValid())
			{
				const UVoxelData& VoxelData = Item.GetData();
				if(!MeshVoxelNatures.Contains(VoxelData.Nature))
				{
					MeshVoxelNatures.Add(VoxelData.Nature);
				}
			}
		}, true);
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
	return Neighbors[InDirection] ? Neighbors[InDirection] : Module->SpawnChunk(Index + FMathHelper::DirectionToIndex(InDirection), bAddToQueue);
}

FIndex AVoxelChunk::GetWorldIndex() const
{
	return Index * Module->GetWorldData().ChunkSize;
}

FVector AVoxelChunk::GetWorldLocation() const
{
	return UVoxelModuleStatics::ChunkIndexToLocation(Index);
}
