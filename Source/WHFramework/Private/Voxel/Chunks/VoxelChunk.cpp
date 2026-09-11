

#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Common/CommonModuleStatics.h"
#include "Common/CommonModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Main/MainModule.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "Voxel/Generators/VoxelTownGenerator.h"
#include "Voxel/Root/VoxelRoot.h"

bool UVoxelChunk::TryMakeVoxelMapCell(const FVoxelItem& InItem, int32 InHeight, FVoxelMapCell& OutCell)
{
	OutCell = FVoxelMapCell();
	if(InItem.IsUnknown() || !InItem.IsValid()) return false;

	const UVoxelData& VoxelData = InItem.GetData();
	if(VoxelData.MeshDatas.IsEmpty()) return false;
	const FVoxelMeshData& MeshData = VoxelData.GetMeshData(InItem);
	const int32 UpFace = static_cast<int32>(EDirectionN::Up);
	if(MeshData.bCustomMesh || !MeshData.MeshUVDatas.IsValidIndex(UpFace) || !MeshData.MeshUVDatas[UpFace].Texture) return false;

	const FVoxelMeshUVData& UVData = MeshData.MeshUVDatas[UpFace];
	OutCell.Texture = UVData.Texture;
	OutCell.UVCorner = UVData.UVCorner;
	OutCell.UVSpan = UVData.UVSpan;
	OutCell.Angle = InItem.Angle;
	OutCell.Height = InHeight;
	return true;
}

UVoxelChunk::UVoxelChunk()
{
	MeshComponents = TMap<EVoxelNature, UVoxelMeshComponent*>();

	Batch = -1;
	Index = FIndex::ZeroIndex;
	bBuilded = false;
	BuildStage.Store(0);
	bGenerated = false;
	bChanged = false;
	Module = nullptr;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	VoxelMapChunk = FVoxelMapChunk();
	TopographyMap = TMap<FIndex, FVoxelTopography>();
	VoxelUpdateIndices = TSet<FIndex>();
	Neighbors = TMap<EDirectionN, UVoxelChunk*>();
	ITER_DIRECTION(Iter, Neighbors.Add(Iter); )

	bNeedCreateMesh = false;
}

void UVoxelChunk::OnSpawn_Implementation(
	const FParameter& InParameter)
{
	
}

void UVoxelChunk::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SaveData();
	Module->VoxelUpdateChunkIndices.Remove(Index);
	
	Index = FIndex::ZeroIndex;
	Batch = -1;
	bBuilded = false;
	BuildStage.Store(0);
	bGenerated = false;
	bChanged = false;

	DestroyMeshComponents();

	DestroySceneActors();

	BreakNeighbors();

	for(auto& Iter : VoxelMap) DestroyAuxiliary(Iter.Value);
	VoxelMap.Empty();
	{
		FScopeLock ScopeLock(&VoxelMapCriticalSection);
		VoxelMapChunk = FVoxelMapChunk();
	}

	TopographyMap.Empty();
	VoxelUpdateIndices.Empty();
	bNeedCreateMesh = false;
}

void UVoxelChunk::LoadData(FSaveData* InSaveData, EPhase InPhase)
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
	RebuildVoxelMap();
	BuildStage.Store(Module->ChunkQueues[EVoxelWorldState::MapBuilding].Queues.Num());
}

FSaveData* UVoxelChunk::ToData()
{
	FVoxelChunkSaveData& SaveData = GetMutableSaveData<FVoxelChunkSaveData>();
	SaveData = FVoxelChunkSaveData();

	SaveData.Index = Index;
	SaveData.bChanged = bChanged;

	if(bChanged)
	{
		for(auto& Iter : VoxelMap)
		{
			FVoxelItem& Item = Iter.Value;
			if(Item.IsValid())
			{
				SaveData.VoxelDatas.Appendf(TEXT("%s|"), *Item.ToSaveData(false, true));
			}
		}
		SaveData.VoxelDatas.RemoveFromEnd(TEXT("|"));

		for(auto& Iter : TopographyMap)
		{
			SaveData.TopographyDatas.Appendf(TEXT("%s|"), *Iter.Value.ToSaveData());
		}
		SaveData.TopographyDatas.RemoveFromEnd(TEXT("|"));
	}
	else if(const FVoxelChunkSaveData* ChunkData = Module->GetWorldData().GetChunkData(Index))
	{
		SaveData.VoxelDatas = ChunkData->VoxelDatas;
		SaveData.TopographyDatas = ChunkData->TopographyDatas;
	}

	SaveData.AuxiliaryDatas.Reset();
	SaveData.PickUpDatas.Reset();
	for(auto& Iter : SceneActorMap)
	{
		if(AVoxelAuxiliary* Auxiliary = Cast<AVoxelAuxiliary>(Iter.Value))
		{
			SaveData.AuxiliaryDatas.Add(Auxiliary->GetSaveDataRef<FVoxelAuxiliarySaveData>(true));
		}
		else if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(Iter.Value))
		{
			SaveData.PickUpDatas.Add(PickUp->GetSaveDataRef<FPickUpSaveData>(true));
		}
	}

	return &SaveData;
}

void UVoxelChunk::SaveData()
{
	if(bGenerated)
	{
		Module->GetWorldData().SetChunkData(Index, GetSaveData<FVoxelChunkSaveData>(true));
	}
}

void UVoxelChunk::Initialize(UVoxelModule* InModule, FIndex InIndex, int32 InBatch)
{
	Module = InModule;
	Index = InIndex;
	Batch = InBatch;
	const int32 ExpectedColumnHeight = FMath::Min(Module->GetWorldData().SkyHeight, FMath::Max(Module->GetWorldData().SeaLevel + 8, 32));
	VoxelMap.Reserve(FMath::Max(FMath::RoundToInt(Module->GetWorldData().ChunkSize.X * Module->GetWorldData().ChunkSize.Y) * ExpectedColumnHeight, 1));
	{
		FScopeLock ScopeLock(&VoxelMapCriticalSection);
		VoxelMapChunk.Size = FIntPoint(
			FMath::RoundToInt(Module->GetWorldData().ChunkSize.X),
			FMath::RoundToInt(Module->GetWorldData().ChunkSize.Y));
		VoxelMapChunk.CellSize = Module->GetWorldData().BlockSize;
		VoxelMapChunk.Origin = FVector2D(Index.X * VoxelMapChunk.Size.X, Index.Y * VoxelMapChunk.Size.Y) * VoxelMapChunk.CellSize;
		VoxelMapChunk.Cells.SetNum(FMath::Max(VoxelMapChunk.Size.X * VoxelMapChunk.Size.Y, 0));
	}
	
	ITER_INDEX2D(VoxelIndex, Module->GetWorldData().ChunkSize, false,
		FVoxelTopography Topography;
		Topography.Index = VoxelIndex;
		TopographyMap.Add(VoxelIndex, Topography);
	)
	UpdateNeighbors();
}

void UVoxelChunk::Generate(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Primary))
	{
		CreateMesh();
		for(auto& Iter : VoxelMap)
		{
			FVoxelItem& Item = Iter.Value;
			if(Item.IsValid())
			{
				Item.OnGenerate();
				const EVoxelType VoxelType = Item.GetVoxelType();
				if(VoxelType == EVoxelType::Oak_Sapling || VoxelType == EVoxelType::Birch_Sapling)
				{
					VoxelUpdateIndices.Add(Iter.Key);
				}
			}
		}
		GenerateSceneActors();
		bGenerated = true;
		if(!VoxelUpdateIndices.IsEmpty()) Module->VoxelUpdateChunkIndices.Add(Index);
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bNeedCreateMesh) return;
		bNeedCreateMesh = true;
		SpawnMeshComponents();
		const FIndex GenerateIndex = Index;
		TWeakObjectPtr<UVoxelChunk> Chunk(this);
		AsyncTask(ENamedThreads::GameThread, [Chunk, GenerateIndex]()
		{
			if(!Chunk.IsValid() || Chunk->Index != GenerateIndex || !Chunk->bGenerated) return;
			Chunk->BuildMesh();
			Chunk->CreateMesh();
			Chunk->bNeedCreateMesh = false;
		});
	}
}

void UVoxelChunk::CreateMesh()
{
	for(auto& Iter : MeshComponents)
	{
		Iter.Value->CreateMesh();
	}
}

void UVoxelChunk::CreateMesh(EVoxelNature InNature)
{
	if(MeshComponents.Contains(InNature)) MeshComponents[InNature]->CreateMesh();
}

void UVoxelChunk::ClearMap(bool bGenerate)
{
	for(auto& Iter : VoxelMap) DestroyAuxiliary(Iter.Value);
	VoxelMap.Empty();
	{
		FScopeLock ScopeLock(&VoxelMapCriticalSection);
		for(FVoxelMapCell& Cell : VoxelMapChunk.Cells) Cell = FVoxelMapCell();
	}
	if(bGenerate) Generate(EPhase::Lesser);
}

void UVoxelChunk::BuildMap(int32 InStage)
{
	BuildStage.Store(InStage);
	if(InStage == Module->GetChunkQueues(EVoxelWorldState::MapBuilding).Queues.Num())
	{
		bBuilded = true;
		RebuildVoxelMap();
	}
}

void UVoxelChunk::GetVoxelItemsSnapshot(TArray<FVoxelItem>& OutVoxelItems)
{
	FScopeLock ScopeLock(&CriticalSection);
	VoxelMap.GenerateValueArray(OutVoxelItems);
}

void UVoxelChunk::BuildMesh()
{
	TArray<FVoxelItem> VoxelItems;
	GetVoxelItemsSnapshot(VoxelItems);
	for(FVoxelItem& Item : VoxelItems)
	{
		if(Item.IsValid())
		{
			GetMeshComponent(Item.GetData().Nature)->BuildVoxel(Item);
		}
	}
}

void UVoxelChunk::BuildMesh(EVoxelNature InNature)
{
	UVoxelMeshComponent* MeshComponent = nullptr;
	if(MeshComponents.Contains(InNature))
	{
		MeshComponent = MeshComponents[InNature];
	}
	else
	{
		MeshComponent = UObjectPoolModuleStatics::SpawnObject<UVoxelMeshComponent>(
			FVoxelMeshComponentSpawnParameter(Module->GetVoxelRoot(), this));
		MeshComponent->Initialize(EVoxelScope::Chunk, InNature);
		MeshComponents.Add(InNature, MeshComponent);
	}
	TArray<FVoxelItem> VoxelItems;
	GetVoxelItemsSnapshot(VoxelItems);
	for(FVoxelItem& Item : VoxelItems)
	{
		if(Item.IsValid() && Item.GetData().Nature == InNature)
		{
			MeshComponent->BuildVoxel(Item);
		}
	}
}

void UVoxelChunk::GenerateNeighbors(FIndex InIndex, EPhase InPhase)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InPhase);
}

void UVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, EPhase InPhase)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX <= 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Backward))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InX >= WorldData.ChunkSize.X - 1)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Forward))
		{
			Chunk->Generate(InPhase);
		}
	}
	if(InY <= 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Left))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y - 1)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Right))
		{
			Chunk->Generate(InPhase);
		}
	}
}

void UVoxelChunk::UpdateNeighbors()
{
	ITER_DIRECTION(Direction,
		Neighbors[Direction] = Module->GetChunkByIndex(Index + FMathHelper::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[FMathHelper::InvertDirection(Direction)] = this;
		}
	)
}

void UVoxelChunk::BreakNeighbors()
{
	ITER_DIRECTION(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[FMathHelper::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool UVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	const auto& WorldData = Module->GetWorldData();
	return InIndex.X >= 0 && InIndex.X < WorldData.ChunkSize.X &&
		InIndex.Y >= 0 && InIndex.Y < WorldData.ChunkSize.Y &&
		InIndex.Z >= 0 && InIndex.Z < WorldData.SkyHeight;
}

bool UVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	const auto& WorldData = Module->GetWorldData();
	return InLocation.X >= GetWorldLocation().X && InLocation.X < GetWorldLocation().X + WorldData.GetChunkRealSize().X &&
		InLocation.Y >= GetWorldLocation().Y && InLocation.Y < GetWorldLocation().Y + WorldData.GetChunkRealSize().Y &&
		InLocation.Z >= GetWorldLocation().Z && InLocation.Z < GetWorldLocation().Z + WorldData.GetWorldRealHeight();
}

FIndex UVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	InLocation = (InLocation - (bWorldSpace ? GetWorldLocation() : FVector::ZeroVector)) / Module->GetWorldData().BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), FMath::FloorToInt(InLocation.Z));
}

FVector UVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	return InIndex.ToVector() * Module->GetWorldData().BlockSize + (bWorldSpace ? GetWorldLocation() : FVector::ZeroVector);
}

FIndex UVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * Module->GetWorldData().ChunkSize;
}

FIndex UVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * Module->GetWorldData().ChunkSize;
}

bool UVoxelChunk::LocalIndexToNeighbor(FIndex InIndex, EDirectionN& OutDirection) const
{
	const auto& WorldData = Module->GetWorldData();
	if(InIndex.X < 0)
	{
		OutDirection = EDirectionN::Backward;
		return true;
	}
	else if(InIndex.X >= WorldData.ChunkSize.X)
	{
		OutDirection = EDirectionN::Forward;
		return true;
	}
	else if(InIndex.Y < 0)
	{
		OutDirection = EDirectionN::Left;
		return true;
	}
	else if(InIndex.Y >= WorldData.ChunkSize.Y)
	{
		OutDirection = EDirectionN::Right;
		return true;
	}
	return false;
}

bool UVoxelChunk::HasVoxel(FIndex InIndex, bool bSafe)
{
	FScopeLock ScopeLock(&CriticalSection);

	return VoxelMap.Contains(InIndex) && (!bSafe || VoxelMap.FindChecked(InIndex).IsValid());
}

bool UVoxelChunk::HasVoxel(int32 InX, int32 InY, int32 InZ, bool bSafe)
{
	return HasVoxel(FIndex(InX, InY, InZ), bSafe);
}

bool UVoxelChunk::HasVoxelComplex(FIndex InIndex, bool bSafe)
{
	return HasVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, bSafe);
}

bool UVoxelChunk::HasVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bSafe)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Backward))
		{
			return Chunk->HasVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, bSafe);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Forward))
		{
			return Chunk->HasVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, bSafe);
		}
	}
	else if(InY < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Left))
		{
			return Chunk->HasVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, bSafe);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Right))
		{
			return Chunk->HasVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, bSafe);
		}
	}
	else
	{
		return HasVoxel(InX, InY, InZ, bSafe);
	}
	return false;
}

FVoxelItem& UVoxelChunk::GetVoxel(FIndex InIndex, bool bMainPart)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& UVoxelChunk::GetVoxel(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	FScopeLock ScopeLock(&CriticalSection);

	if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		if(bMainPart) return VoxelMap.FindChecked(VoxelIndex).GetMain();
		return VoxelMap.FindChecked(VoxelIndex);
	}
	return FVoxelItem::Empty;
}

FVoxelItem UVoxelChunk::GetVoxelSnapshot(FIndex InIndex)
{
	FScopeLock ScopeLock(&CriticalSection);
	if(!VoxelMap.Contains(InIndex)) return FVoxelItem::Empty;
	return VoxelMap.FindChecked(InIndex);
}

FVoxelItem& UVoxelChunk::GetVoxelComplex(FIndex InIndex, bool bMainPart)
{
	return GetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& UVoxelChunk::GetVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Backward))
		{
			return Chunk->GetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Forward))
		{
			return Chunk->GetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Left))
		{
			return Chunk->GetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Right))
		{
			return Chunk->GetVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else
	{
		return GetVoxel(InX, InY, InZ, bMainPart);
	}
	return FVoxelItem::Empty;
}

bool UVoxelChunk::CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange/* = FVector::OneVector*/)
{
	ITER_INDEX(Iter, InRange, false,
		if(!GetVoxelComplex(InIndex + Iter).IsReplaceable(InVoxelItem))
		{
			return true;
		}
	)
	return false;
}

bool UVoxelChunk::CheckVoxelAdjacent(FIndex InIndex, EDirectionN InDirection)
{
	return CheckVoxelAdjacent(GetVoxel(InIndex), InDirection);
}

bool UVoxelChunk::CheckVoxelAdjacent(const FVoxelItem& InVoxelItem, EDirectionN InDirection)
{
	const FIndex AdjacentIndex = FMathHelper::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Angle);
	
	if(!InVoxelItem.IsValid() || AdjacentIndex.Z < 0 ||
		(AdjacentIndex.Z == 0 && UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Prefab)) return true;
	
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

bool UVoxelChunk::CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange, bool bFromCenter, bool bIgnoreBottom, bool bOnTheChunk)
{
	ITER_INDEX(Iter1, InRange, bFromCenter,
		ITER_DIRECTION(Iter2, 
			if(!bIgnoreBottom || Iter2 != EDirectionN::Down)
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

void UVoxelChunk::SetVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe)
{
	FScopeLock ScopeLock(&CriticalSection);
	if(!bSafe || InVoxelItem.IsValid())
	{
		FVoxelItem VoxelItem = InVoxelItem;
		VoxelItem.Chunk = this;
		VoxelItem.Index = InIndex;
		VoxelMap.Add(InIndex, VoxelItem);
	}
	else if(VoxelMap.Contains(InIndex))
	{
		VoxelMap.Remove(InIndex);
	}
	if(bBuilded || bGenerated)
	{
		const FVoxelItem* StoredItem = VoxelMap.Find(InIndex);
		UpdateVoxelMapColumn(InIndex, StoredItem ? *StoredItem : InVoxelItem);
	}
}

void UVoxelChunk::SetVoxel(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bSafe)
{
	return SetVoxel(FIndex(InX, InY, InZ), InVoxelItem, bSafe);
}

void UVoxelChunk::ReadVoxelMap(TFunctionRef<void(const FVoxelMapChunk&)> InReader) const
{
	FScopeLock ScopeLock(&VoxelMapCriticalSection);
	InReader(VoxelMapChunk);
}

void UVoxelChunk::RebuildVoxelMap()
{
	FScopeLock VoxelScopeLock(&CriticalSection);
	FScopeLock MapScopeLock(&VoxelMapCriticalSection);
	for(FVoxelMapCell& Cell : VoxelMapChunk.Cells) Cell = FVoxelMapCell();
	for(const auto& Pair : VoxelMap)
	{
		const FIndex& LocalIndex = Pair.Key;
		if(LocalIndex.X < 0 || LocalIndex.Y < 0 || LocalIndex.X >= VoxelMapChunk.Size.X || LocalIndex.Y >= VoxelMapChunk.Size.Y) continue;
		const int32 CellIndex = LocalIndex.Y * VoxelMapChunk.Size.X + LocalIndex.X;
		FVoxelMapCell Candidate;
		if(TryMakeVoxelMapCell(Pair.Value, LocalIndex.Z, Candidate) &&
			VoxelMapChunk.Cells.IsValidIndex(CellIndex) && Candidate.Height > VoxelMapChunk.Cells[CellIndex].Height)
		{
			VoxelMapChunk.Cells[CellIndex] = Candidate;
		}
	}
}

void UVoxelChunk::UpdateVoxelMapColumn(const FIndex& InIndex, const FVoxelItem& InVoxelItem)
{
	if(InIndex.X < 0 || InIndex.Y < 0 || InIndex.X >= VoxelMapChunk.Size.X || InIndex.Y >= VoxelMapChunk.Size.Y) return;

	FScopeLock ScopeLock(&VoxelMapCriticalSection);
	const int32 CellIndex = InIndex.Y * VoxelMapChunk.Size.X + InIndex.X;
	if(!VoxelMapChunk.Cells.IsValidIndex(CellIndex)) return;

	FVoxelMapCell& Cell = VoxelMapChunk.Cells[CellIndex];
	if(InIndex.Z < Cell.Height) return;

	FVoxelMapCell ChangedCell;
	const bool bVisible = TryMakeVoxelMapCell(InVoxelItem, InIndex.Z, ChangedCell);
	if(InIndex.Z > Cell.Height)
	{
		if(bVisible) Cell = ChangedCell;
	}
	else if(bVisible)
	{
		Cell = ChangedCell;
	}
	else
	{
		Cell = FVoxelMapCell();
		for(int32 Height = InIndex.Z - 1; Height >= 0; --Height)
		{
			const FVoxelItem* Item = VoxelMap.Find(FIndex(InIndex.X, InIndex.Y, Height));
			if(Item && TryMakeVoxelMapCell(*Item, Height, Cell)) break;
		}
	}
}

bool UVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = false;
	const EVoxelType PreviousType = GetVoxel(InIndex).GetVoxelType();
	if(InVoxelItem.IsValid())
	{
		if(IsOnTheChunk(InIndex))
		{
			SetVoxel(InIndex, InVoxelItem);
			if(bGenerate) VoxelMap.FindChecked(InIndex).OnGenerate(InAgent);
			bSuccess = true;
		}
	}
	else
	{
		if(HasVoxel(InIndex))
		{
			FVoxelItem VoxelItem = VoxelMap.FindChecked(InIndex);
			SetVoxel(InIndex, FVoxelItem::Empty, true);
			if(bGenerate) VoxelItem.OnDestroy(InAgent);
			bSuccess = true;
		}
	}
	if(bSuccess)
	{
		if(bGenerate)
		{
			bool bUpdateLiquid = PreviousType == EVoxelType::Water || GetVoxel(InIndex).GetVoxelType() == EVoxelType::Water;
			if(!bUpdateLiquid)
			{
				ITER_DIRECTION(Direction,
					if(GetVoxelComplex(InIndex + FMathHelper::DirectionToIndex(Direction)).GetVoxelType() == EVoxelType::Water)
					{
						bUpdateLiquid = true;
						break;
					}
				)
			}
			if(bUpdateLiquid) Module->AddToVoxelLiquidUpdateQueue(LocalIndexToWorld(InIndex));
			Generate(EPhase::Lesser);
			GenerateNeighbors(InIndex, EPhase::Lesser);
			if(InAgent) bChanged = true;
		}
	}
	return bSuccess;
}

bool UVoxelChunk::SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelSample(FIndex(InX, InY, InZ), InVoxelItem, bGenerate, InAgent);
}

bool UVoxelChunk::SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, InVoxelItem, bGenerate, InAgent);
}

bool UVoxelChunk::SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	const auto& WorldData = Module->GetWorldData();
	if(InX < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Backward))
		{
			return Chunk->SetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Forward))
		{
			return Chunk->SetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Left))
		{
			return Chunk->SetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(UVoxelChunk* Chunk = GetNeighbor(EDirectionN::Right))
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
				TMap<FIndex, FVoxelItem> ComplexVoxelMap;
				ComplexVoxelMap.Emplace(VoxelIndex, InVoxelItem);
				ITER_ARRAY(VoxelData.PartDatas, PartData,
					FVoxelItem PartItem = FVoxelItem::Empty;
					if(InVoxelItem.IsValid())
					{
						PartItem.ID = PartData->GetPrimaryAssetId();
						PartItem.Angle = InVoxelItem.Angle;
					}
					ComplexVoxelMap.Emplace(VoxelIndex + FMathHelper::RotateIndex(PartData->PartIndex, VoxelItem.Angle), PartItem);
				)
				return SetVoxelComplex(ComplexVoxelMap, bGenerate, true, InAgent);
			}
			else
			{
				return SetVoxelSample(VoxelIndex, InVoxelItem, bGenerate, InAgent);
			}
		}
	}
	return false;
}

bool UVoxelChunk::SetVoxelComplex(const TMap<FIndex, FVoxelItem>& InVoxelMap, bool bGenerate, bool bFirstSample, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = true;
	TArray<EDirectionN> _Neighbors;
	for(const auto& Iter : InVoxelMap)
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
			EDirectionN _Neighbor;
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

int32 UVoxelChunk::UpdateVoxels(int32 InMaxCount, TSet<FIndex>& OutChangedChunkIndices)
{
	TArray<FIndex> UpdateIndices;
	for(auto Iter = VoxelUpdateIndices.CreateIterator(); Iter && UpdateIndices.Num() < InMaxCount; ++Iter)
	{
		UpdateIndices.Add(*Iter);
		Iter.RemoveCurrent();
	}
	for(const FIndex& LocalIndex : UpdateIndices)
	{
		const FIndex WorldIndex = LocalIndexToWorld(LocalIndex);
		const FVoxelItem& VoxelItem = GetVoxel(LocalIndex);
		switch(VoxelItem.GetVoxelType())
		{
			case EVoxelType::Oak_Sapling:
			case EVoxelType::Birch_Sapling:
				UpdateSapling(WorldIndex, VoxelItem.GetVoxelType(), OutChangedChunkIndices);
				break;
			default:
				break;
		}
	}
	return UpdateIndices.Num();
}


void UVoxelChunk::UpdateSapling(FIndex InIndex, EVoxelType InVoxelType, TSet<FIndex>& OutChangedChunkIndices)
{
	auto& WorldData = Module->GetWorldData();
	if(WorldData.RandomStream.FRand() > 0.001f)
	{
		Module->AddToVoxelUpdateQueue(InIndex);
		return;
	}

	const FVoxelItem& GroundItem = Module->GetVoxelByIndex(InIndex + FIndex(0, 0, -1));
	if(GroundItem.GetVoxelType() != EVoxelType::Grass && GroundItem.GetVoxelType() != EVoxelType::Dirt)
	{
		Module->AddToVoxelUpdateQueue(InIndex);
		return;
	}

	const int32 Height = WorldData.RandomStream.RandRange(4, 9);
	const int32 LeafRadius = Height >= 7 ? 3 : 2;
	const EVoxelType WoodType = InVoxelType == EVoxelType::Birch_Sapling ? EVoxelType::Birch : EVoxelType::Oak;
	const EVoxelType LeafType = InVoxelType == EVoxelType::Birch_Sapling ? EVoxelType::Birch_Leaves : EVoxelType::Oak_Leaves;
	for(int32 Z = 0; Z < Height; ++Z)
	{
		const FIndex TreeIndex = InIndex + FIndex(0, 0, Z);
		const UVoxelChunk* TreeChunk = Module->GetChunkByVoxelIndex(TreeIndex);
		if(!TreeChunk || !TreeChunk->IsGenerated() || (Z > 0 && Module->GetVoxelByIndex(TreeIndex).IsValid()))
		{
			Module->AddToVoxelUpdateQueue(InIndex);
			return;
		}
	}
	for(int32 Z = -2; Z <= 2; ++Z)
	{
		const int32 LayerRadius = FMath::Max(LeafRadius - FMath::Abs(Z) / 2, 1);
		for(int32 X = -LayerRadius; X <= LayerRadius; ++X)
		{
			for(int32 Y = -LayerRadius; Y <= LayerRadius; ++Y)
			{
				if(X * X + Y * Y > LayerRadius * LayerRadius + 1) continue;
				const FIndex LeafIndex = InIndex + FIndex(X, Y, Height - 1 + Z);
				const UVoxelChunk* LeafChunk = Module->GetChunkByVoxelIndex(LeafIndex);
				if(!LeafChunk || !LeafChunk->IsGenerated())
				{
					Module->AddToVoxelUpdateQueue(InIndex);
					return;
				}
			}
		}
	}

	TMap<FIndex, FVoxelItem> VoxelUpdates;
	for(int32 Z = 0; Z < Height; ++Z)
	{
		const FIndex TreeIndex = InIndex + FIndex(0, 0, Z);
		VoxelUpdates.Add(TreeIndex, FVoxelItem(WoodType, FIndex::ZeroIndex, nullptr, Z == 0 ? FString(TEXT("R")) : FString::Printf(TEXT("T%s"), *(InIndex - TreeIndex).ToString())));
	}
	for(int32 Z = -2; Z <= 2; ++Z)
	{
		const int32 LayerRadius = FMath::Max(LeafRadius - FMath::Abs(Z) / 2, 1);
		for(int32 X = -LayerRadius; X <= LayerRadius; ++X)
		{
			for(int32 Y = -LayerRadius; Y <= LayerRadius; ++Y)
			{
				if(X * X + Y * Y > LayerRadius * LayerRadius + 1) continue;
				const FIndex LeafIndex = InIndex + FIndex(X, Y, Height - 1 + Z);
				if(!VoxelUpdates.Contains(LeafIndex) && !Module->GetVoxelByIndex(LeafIndex).IsValid())
				{
					VoxelUpdates.Add(LeafIndex, FVoxelItem(LeafType, FIndex::ZeroIndex, nullptr, FString::Printf(TEXT("T%s"), *(InIndex - LeafIndex).ToString())));
				}
			}
		}
	}
	Module->ApplyVoxelUpdates(VoxelUpdates, OutChangedChunkIndices);
}

void UVoxelChunk::DestroyTree(FIndex InIndex)
{
	const FIndex RootIndex = LocalIndexToWorld(InIndex);
	TMap<FIndex, FVoxelItem> VoxelUpdates;
	for(int32 X = -4; X <= 4; ++X)
	{
		for(int32 Y = -4; Y <= 4; ++Y)
		{
			for(int32 Z = 0; Z <= 12; ++Z)
			{
				const FIndex VoxelIndex = RootIndex + FIndex(X, Y, Z);
				if(!Module->GetChunkByVoxelIndex(VoxelIndex)) continue;
				const FVoxelItem& VoxelItem = Module->GetVoxelByIndex(VoxelIndex);
				const EVoxelType VoxelType = VoxelItem.GetVoxelType();
				if((VoxelType == EVoxelType::Oak || VoxelType == EVoxelType::Oak_Leaves || VoxelType == EVoxelType::Birch || VoxelType == EVoxelType::Birch_Leaves) &&
					VoxelItem.Data.StartsWith(TEXT("T")) && VoxelIndex + FIndex(VoxelItem.Data.Mid(1)) == RootIndex)
				{
					if((VoxelType == EVoxelType::Oak || VoxelType == EVoxelType::Birch) && UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Prefab)
					{
						const UVoxelData& VoxelData = VoxelItem.GetData();
						UAbilityModuleStatics::SpawnAbilityPickUp(FAbilityItem(VoxelData.GatherData ? VoxelData.GatherData->GetPrimaryAssetId() : VoxelData.GetPrimaryAssetId(), 1),
							VoxelItem.GetLocation() + VoxelData.GetRange(VoxelItem.Angle) * Module->GetWorldData().BlockSize * 0.5f, VoxelItem.Chunk);
					}
					VoxelUpdates.Add(VoxelIndex, FVoxelItem::Empty);
				}
			}
		}
	}
	TSet<FIndex> ChangedChunkIndices;
	Module->ApplyVoxelUpdates(VoxelUpdates, ChangedChunkIndices);
	for(const FIndex& ChunkIndex : ChangedChunkIndices)
	{
		if(UVoxelChunk* Chunk = Module->GetChunkByIndex(ChunkIndex)) Chunk->Generate(EPhase::Lesser);
	}
}

FVoxelTopography& UVoxelChunk::GetTopography(FIndex InIndex)
{
	InIndex.Z = 0;
	return TopographyMap.FindChecked(InIndex);
}

FVoxelTopography& UVoxelChunk::GetTopography(int32 InX, int32 InY, int32 InZ)
{
	return GetTopography(FIndex(InX, InY, InZ));
}

void UVoxelChunk::SetTopography(FIndex InIndex, const FVoxelTopography& InTopography)
{
	InIndex.Z = 0;
	TopographyMap.Emplace(InIndex, InTopography);
}

void UVoxelChunk::SetTopography(int32 InX, int32 InY, int32 InZ, const FVoxelTopography& InTopography)
{
	SetTopography(FIndex(InX, InY, InZ), InTopography);
}

bool UVoxelChunk::HasSceneActor(const FString& InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(FGuid(InID))) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No SceneActor, Actor name: %s"), *InID), EDC_Voxel, EDV_Error);
	return false;
}

AActor* UVoxelChunk::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActor(InID, bEnsured))
	{
		return SceneActorMap[FGuid(InID)];
	}
	return nullptr;
}

bool UVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(!SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor)))
	{
		SceneActorMap.Add(ISceneActorInterface::Execute_GetActorID(InActor), InActor);
		if(ISceneActorInterface::Execute_GetContainer(InActor) != this)
		{
			if(ISceneActorInterface::Execute_GetContainer(InActor))
			{
				ISceneActorInterface::Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
			}
			ISceneActorInterface::Execute_SetContainer(InActor, this);
		}
		return true;
	}
	return false;
}

bool UVoxelChunk::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor)))
	{
		SceneActorMap.Remove(ISceneActorInterface::Execute_GetActorID(InActor));
		if(ISceneActorInterface::Execute_GetContainer(InActor) == this)
		{
			ISceneActorInterface::Execute_SetContainer(InActor, nullptr);
		}
		return true;
	}
	return false;
}

void UVoxelChunk::GenerateSceneActors()
{
	auto& WorldData = Module->GetWorldData();
	
	if(WorldData.IsExistChunkData(Index))
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

void UVoxelChunk::LoadSceneActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();

	for(auto& Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleStatics::SpawnAbilityPickUp(&Iter, this);
	}
}

void UVoxelChunk::SpawnSceneActors()
{
	
}

void UVoxelChunk::DestroySceneActors()
{
	for(auto& Iter : TMap(SceneActorMap))
	{
		if(UCommonModuleStatics::GetPlayerPawn() != Iter.Value)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		}
		SceneActorMap.Remove(Iter.Key);
	}
}

AVoxelAuxiliary* UVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const auto& VoxelData = InVoxelItem.GetData();
		if(VoxelData.AuxiliaryClass && VoxelData.IsMainPart())
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(VoxelData.AuxiliaryClass))
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

void UVoxelChunk::DestroyAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && InVoxelItem.Auxiliary)
	{
		UObjectPoolModuleStatics::DespawnObject(InVoxelItem.Auxiliary);
		InVoxelItem.Auxiliary = nullptr;
	}
}

void UVoxelChunk::SpawnMeshComponents(int32 InStage)
{
	if(InStage & 1)
	{
		TArray<FVoxelItem> VoxelItems;
		GetVoxelItemsSnapshot(VoxelItems);
		for(FVoxelItem& Item : VoxelItems)
		{
			if(Item.IsValid())
			{
				const UVoxelData& VoxelData = Item.GetData();
				if(!MeshVoxelNatures.Contains(VoxelData.Nature))
				{
					MeshVoxelNatures.Add(VoxelData.Nature);
				}
			}
		}
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
				MeshComponent = UObjectPoolModuleStatics::SpawnObject<UVoxelMeshComponent>(
					FVoxelMeshComponentSpawnParameter(Module->GetVoxelRoot(), this));
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

void UVoxelChunk::DestroyMeshComponents()
{
	for(auto& Iter : MeshComponents)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Value);
	}
	MeshComponents.Empty();
}

UVoxelMeshComponent* UVoxelChunk::GetMeshComponent(EVoxelNature InVoxelNature)
{
	return MeshComponents[InVoxelNature];
}

UVoxelChunk* UVoxelChunk::GetOrSpawnNeighbor(EDirectionN InDirection, bool bAddToQueue)
{
	return Neighbors[InDirection] ? Neighbors[InDirection] : Module->SpawnChunk(Index + FMathHelper::DirectionToIndex(InDirection), bAddToQueue);
}

FIndex UVoxelChunk::GetWorldIndex() const
{
	return Index * Module->GetWorldData().ChunkSize;
}

FVector UVoxelChunk::GetWorldLocation() const
{
	return UVoxelModuleStatics::ChunkIndexToLocation(Index);
}
