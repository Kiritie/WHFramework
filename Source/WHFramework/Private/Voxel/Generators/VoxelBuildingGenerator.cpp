#include "Voxel/Generators/VoxelBuildingGenerator.h"

#include "Asset/AssetModuleStatics.h"
#include "Math/MathHelper.h"
#include "Misc/ScopeRWLock.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Voxels/Data/VoxelData.h"

UVoxelBuildingGenerator::UVoxelBuildingGenerator()
{
	Seed = 673;
	SpawnRate = 0.00025f;

	GenerateDatas = {
		FVoxelBuildingGenerateData(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Dungeon_1")), 1.f),
	};
}

void UVoxelBuildingGenerator::Initialize(UVoxelModule* InModule)
{
	Super::Initialize(InModule);
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 HalfChunkX = FMath::FloorToInt(ChunkSize.X * 0.5f);
	const int32 HalfChunkY = FMath::FloorToInt(ChunkSize.Y * 0.5f);

	_PrefabAssets.Reserve(GenerateDatas.Num());
	_PrefabCaches.SetNum(GenerateDatas.Num());
	for(int32 DataIndex = 0; DataIndex < GenerateDatas.Num(); ++DataIndex)
	{
		const FVoxelBuildingGenerateData& GenerateData = GenerateDatas[DataIndex];
		UVoxelPrefabData* Prefab = GenerateData.PrefabAsset.IsValid()
			? UAssetModuleStatics::LoadPrimaryAsset<UVoxelPrefabData>(GenerateData.PrefabAsset)
			: nullptr;
		_PrefabAssets.Add(Prefab);
		if(IsValid(Prefab))
		{
			TArray<FString> SerializedVoxelDatas;
			Prefab->VoxelDatas.ParseIntoArray(SerializedVoxelDatas, TEXT("|"));
			TArray<FVoxelItem> SourceItems;
			int32 SourceMinX = MAX_int32;
			int32 SourceMaxX = MIN_int32;
			int32 SourceMinY = MAX_int32;
			int32 SourceMaxY = MIN_int32;
			for(const FString& SerializedVoxelData : SerializedVoxelDatas)
			{
				FVoxelItem Item(SerializedVoxelData, true);
				if(!Item.IsValid()) continue;
				SourceItems.Add(Item);
				SourceMinX = FMath::Min(SourceMinX, Item.Index.X);
				SourceMaxX = FMath::Max(SourceMaxX, Item.Index.X);
				SourceMinY = FMath::Min(SourceMinY, Item.Index.Y);
				SourceMaxY = FMath::Max(SourceMaxY, Item.Index.Y);
			}
			if(SourceItems.IsEmpty()) continue;

			TArray<FIndex> SourceEntranceIndices;
			switch(GenerateData.FrontDirection)
			{
				case ERightAngle::RA_0: for(int32 Y = SourceMinY; Y <= SourceMaxY; ++Y) SourceEntranceIndices.Emplace(SourceMaxX, Y, 0); break;
				case ERightAngle::RA_90: for(int32 X = SourceMinX; X <= SourceMaxX; ++X) SourceEntranceIndices.Emplace(X, SourceMaxY, 0); break;
				case ERightAngle::RA_180: for(int32 Y = SourceMinY; Y <= SourceMaxY; ++Y) SourceEntranceIndices.Emplace(SourceMinX, Y, 0); break;
				default: for(int32 X = SourceMinX; X <= SourceMaxX; ++X) SourceEntranceIndices.Emplace(X, SourceMinY, 0); break;
			}

			FVoxelBuildingPrefabCache& PrefabCache = _PrefabCaches[DataIndex];
			PrefabCache.ClearHeight = FMath::CeilToInt(Prefab->VoxelSize.Z);
			PrefabCache.Rotations.SetNum(4);
			for(int32 Rotation = 0; Rotation < 4; ++Rotation)
			{
				const ERightAngle RotationAngle = static_cast<ERightAngle>(Rotation);
				const FIndex RotationOffset = UVoxelModuleStatics::RightAngleToVoxelIndex(RotationAngle);
				FVoxelBuildingRotationCache& RotationCache = PrefabCache.Rotations[Rotation];
				RotationCache.MinX = MAX_int32;
				RotationCache.MaxX = MIN_int32;
				RotationCache.MinY = MAX_int32;
				RotationCache.MaxY = MIN_int32;
				for(const FVoxelItem& SourceItem : SourceItems)
				{
					FVoxelItem Item = SourceItem;
					Item.Index = FMathHelper::RotateIndex(SourceItem.Index, RotationAngle) + RotationOffset;
					if(Item.GetData().bRotatable) Item.Angle = FMathHelper::CombineRightAngle(Item.Angle, RotationAngle);
					const FIndex SliceIndex(
						FMath::FloorToInt(static_cast<float>(HalfChunkX + Item.Index.X) / FMath::Max(ChunkSize.X, 1)),
						FMath::FloorToInt(static_cast<float>(HalfChunkY + Item.Index.Y) / FMath::Max(ChunkSize.Y, 1)), 0);
					RotationCache.ChunkSlices.FindOrAdd(SliceIndex).Add(Item);
					RotationCache.MinX = FMath::Min(RotationCache.MinX, Item.Index.X);
					RotationCache.MaxX = FMath::Max(RotationCache.MaxX, Item.Index.X);
					RotationCache.MinY = FMath::Min(RotationCache.MinY, Item.Index.Y);
					RotationCache.MaxY = FMath::Max(RotationCache.MaxY, Item.Index.Y);
					RotationCache.MaxZ = FMath::Max(RotationCache.MaxZ, Item.Index.Z);
				}
				for(const FIndex& EntranceIndex : SourceEntranceIndices)
				{
					RotationCache.EntranceIndices.Add(FMathHelper::RotateIndex(EntranceIndex, RotationAngle) + RotationOffset);
				}
				PrefabCache.ClearHeight = FMath::Max(PrefabCache.ClearHeight, RotationCache.MaxZ);
				const int32 RotationExtent = FMath::Max(
					FMath::Max(FMath::Abs(RotationCache.MinX), FMath::Abs(RotationCache.MaxX)),
					FMath::Max(FMath::Abs(RotationCache.MinY), FMath::Abs(RotationCache.MaxY)));
				_MaxBuildingExtent = FMath::Max(_MaxBuildingExtent, RotationExtent + 2);
			}
		}
	}
}

void UVoxelBuildingGenerator::Generate(UVoxelChunk* InChunk)
{
	if(!InChunk || !Module || GenerateDatas.IsEmpty() || _PrefabAssets.IsEmpty()) return;

	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 MarginX = FMath::CeilToInt(static_cast<float>(_MaxBuildingExtent) / FMath::Max(ChunkSize.X, 1)) + 1;
	const int32 MarginY = FMath::CeilToInt(static_cast<float>(_MaxBuildingExtent) / FMath::Max(ChunkSize.Y, 1)) + 1;
	for(int32 AnchorX = InChunk->GetIndex().X - MarginX; AnchorX <= InChunk->GetIndex().X + MarginX; ++AnchorX)
	{
		for(int32 AnchorY = InChunk->GetIndex().Y - MarginY; AnchorY <= InChunk->GetIndex().Y + MarginY; ++AnchorY)
		{
			const FIndex AnchorChunkIndex(AnchorX, AnchorY, 0);
			const FVector2D RandomKey = AnchorChunkIndex.ToVector2D();
			if(FMathHelper::HashRand(RandomKey, Seed) >= FMath::Clamp(SpawnRate, 0.f, 1.f)) continue;
			const int32 BuildingIndex = SelectBuildingIndex(RandomKey + FVector2D(37.f, -71.f));
			if(BuildingIndex == INDEX_NONE) continue;
			const FIndex AnchorOrigin = Module->ChunkIndexToVoxelIndex(AnchorChunkIndex);
			const int32 CenterX = AnchorOrigin.X + FMath::FloorToInt(ChunkSize.X * 0.5f);
			const int32 CenterY = AnchorOrigin.Y + FMath::FloorToInt(ChunkSize.Y * 0.5f);
			const bool bPlaced = PlaceBuildingSlice(InChunk, AnchorChunkIndex,
				CenterX,
				CenterY,
				BuildingIndex);
			if(bPlaced && AnchorChunkIndex == InChunk->GetIndex())
			{
				const FVoxelBuildingPlacementPlan Plan = GetOrBuildPlacementPlan(AnchorChunkIndex, CenterX, CenterY, BuildingIndex);
				if(!Plan.bValid) continue;
				const FVoxelBuildingRotationCache& RotationCache = _PrefabCaches[BuildingIndex].Rotations[Plan.Rotation];
				FSceneArea Area;
				Area.AreaName = *FString::Printf(TEXT("Structure_%d_%d"), AnchorChunkIndex.X, AnchorChunkIndex.Y);
				const FText BuildingDisplayName = _PrefabAssets[BuildingIndex]->DisplayName.IsEmpty()
					? Module->GetWorldRegionDisplayName(EVoxelRegionType::Building)
					: _PrefabAssets[BuildingIndex]->DisplayName;
				Area.AreaDisplayName = Module->GetWorldAreaDisplayName(FIndex(CenterX, CenterY, Plan.GroundHeight), EVoxelAreaType::Building, BuildingDisplayName);
				Area.AreaType = ESceneAreaType::Default;
				Area.AreaShape = ESceneAreaShape::Box;
				Area.AreaCenter = FVector2D(CenterX + (RotationCache.MinX + RotationCache.MaxX) * 0.5f,
					CenterY + (RotationCache.MinY + RotationCache.MaxY) * 0.5f);
				Area.AreaRadius = FVector2D((RotationCache.MaxX - RotationCache.MinX) * 0.5f + 4.f,
					(RotationCache.MaxY - RotationCache.MinY) * 0.5f + 4.f);
				USceneModuleStatics::AddSceneArea(Area, true);
			}
		}
	}
}

int32 UVoxelBuildingGenerator::SelectBuildingIndex(const FVector2D& InRandomPosition) const
{
	float TotalChance = 0.f;
	for(int32 i = 0; i < GenerateDatas.Num(); ++i)
	{
		if(_PrefabAssets.IsValidIndex(i) && IsValid(_PrefabAssets[i]))
		{
			TotalChance += FMath::Max(GenerateDatas[i].Chance, 0.f);
		}
	}

	if(TotalChance <= 0.f) return INDEX_NONE;

	const float RandomChance = FMathHelper::HashRand(InRandomPosition, Seed + 1) * TotalChance;
	float AccumulatedChance = 0.f;
	int32 LastValidIndex = INDEX_NONE;
	for(int32 i = 0; i < GenerateDatas.Num(); ++i)
	{
		if(!_PrefabAssets.IsValidIndex(i) || !IsValid(_PrefabAssets[i])) continue;

		const float Chance = FMath::Max(GenerateDatas[i].Chance, 0.f);
		if(Chance <= 0.f) continue;

		LastValidIndex = i;
		AccumulatedChance += Chance;
		if(RandomChance < AccumulatedChance)
		{
			return i;
		}
	}
	return LastValidIndex;
}

FVoxelBuildingPlacementPlan UVoxelBuildingGenerator::BuildPlacementPlan(int32 InX, int32 InY, int32 InBuildingIndex) const
{
	FVoxelBuildingPlacementPlan Plan;
	if(!_PrefabCaches.IsValidIndex(InBuildingIndex)) return Plan;
	const FVoxelBuildingPrefabCache& PrefabCache = _PrefabCaches[InBuildingIndex];
	const FVoxelBuildingGenerateData& GenerateData = GenerateDatas[InBuildingIndex];
	if(PrefabCache.Rotations.Num() != 4) return Plan;
	TMap<FIndex, int32> HeightCache;
	auto SampleHeight = [this, &HeightCache](FIndex Index)
	{
		Index.Z = 0;
		if(const int32* Height = HeightCache.Find(Index)) return *Height;
		const int32 Height = Module->SampleTopographyByIndex(Index).Height;
		HeightCache.Add(Index, Height);
		return Height;
	};

	const int32 RandomStartRotation = FMathHelper::HashRandInt(FVector2D(InX, InY), Seed + 2) % 4;
	int32 BestEntranceMaxDrop = MAX_int32;
	int64 BestEntranceTotalDrop = MAX_int64;
	for(int32 RotationOffset = 0; RotationOffset < 4; ++RotationOffset)
	{
		const int32 Rotation = (RandomStartRotation + RotationOffset) % 4;
		const FVoxelBuildingRotationCache& RotationCache = PrefabCache.Rotations[Rotation];
		if(RotationCache.ChunkSlices.IsEmpty()) continue;

		double AverageHeight = 0.0;
		int32 FootprintCount = 0;
		int32 MinHeight = MAX_int32;
		int32 MaxHeight = MIN_int32;
		bool bInvalidTerrain = false;
		for(int32 X = RotationCache.MinX - 1; X <= RotationCache.MaxX + 1; ++X)
		{
			for(int32 Y = RotationCache.MinY - 1; Y <= RotationCache.MaxY + 1; ++Y)
			{
				const FIndex SampleIndex(InX + X, InY + Y, 0);
				const FVoxelTopography Topography = Module->SampleTopographyByIndex(SampleIndex);
				if(Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.BiomeType == EVoxelBiomeType::River || Topography.RegionType == EVoxelRegionType::Ocean || Topography.RegionType == EVoxelRegionType::River || Topography.RegionType == EVoxelRegionType::Lake)
				{
					bInvalidTerrain = true;
					break;
				}
				if(X >= RotationCache.MinX && X <= RotationCache.MaxX && Y >= RotationCache.MinY && Y <= RotationCache.MaxY)
				{
					const int32 Height = SampleHeight(SampleIndex);
					AverageHeight += Height;
					MinHeight = FMath::Min(MinHeight, Height);
					MaxHeight = FMath::Max(MaxHeight, Height);
					++FootprintCount;
				}
			}
			if(bInvalidTerrain) break;
		}
		if(bInvalidTerrain || FootprintCount <= 0 || MaxHeight - MinHeight > GenerateData.MaxTerrainSlope) continue;
		const int32 GroundHeight = FMath::RoundToInt(AverageHeight / FootprintCount);
		if(GroundHeight < Module->GetWorldData().SeaLevel + GenerateData.MinHeightAboveSeaLevel) continue;

		int32 EntranceMaxDrop = 0;
		int64 EntranceTotalDrop = 0;
		for(const FIndex& EntranceIndex : RotationCache.EntranceIndices)
		{
			const int32 EntranceHeight = SampleHeight(FIndex(InX + EntranceIndex.X, InY + EntranceIndex.Y, 0));
			const int32 EntranceDrop = FMath::Max(GroundHeight - EntranceHeight, 0);
			EntranceMaxDrop = FMath::Max(EntranceMaxDrop, EntranceDrop);
			EntranceTotalDrop += EntranceDrop;
		}
		if(EntranceMaxDrop < BestEntranceMaxDrop || (EntranceMaxDrop == BestEntranceMaxDrop && EntranceTotalDrop < BestEntranceTotalDrop))
		{
			Plan.bValid = true;
			Plan.Rotation = Rotation;
			Plan.GroundHeight = GroundHeight;
			BestEntranceMaxDrop = EntranceMaxDrop;
			BestEntranceTotalDrop = EntranceTotalDrop;
		}
	}
	return Plan;
}

FVoxelBuildingPlacementPlan UVoxelBuildingGenerator::GetOrBuildPlacementPlan(FIndex InAnchorChunkIndex, int32 InX, int32 InY, int32 InBuildingIndex)
{
	{
		FReadScopeLock ReadLock(_BuildingPlanCacheLock);
		if(const FVoxelBuildingPlacementPlan* CachedPlan = _BuildingPlanCache.Find(InAnchorChunkIndex)) return *CachedPlan;
	}
	const FVoxelBuildingPlacementPlan NewPlan = BuildPlacementPlan(InX, InY, InBuildingIndex);
	FWriteScopeLock WriteLock(_BuildingPlanCacheLock);
	if(const FVoxelBuildingPlacementPlan* CachedPlan = _BuildingPlanCache.Find(InAnchorChunkIndex)) return *CachedPlan;
	_BuildingPlanCache.Add(InAnchorChunkIndex, NewPlan);
	_BuildingPlanCacheOrder.Add(InAnchorChunkIndex);
	while(_BuildingPlanCacheOrder.Num() > 128)
	{
		_BuildingPlanCache.Remove(_BuildingPlanCacheOrder[0]);
		_BuildingPlanCacheOrder.RemoveAt(0);
	}
	return NewPlan;
}

bool UVoxelBuildingGenerator::PlaceBuildingSlice(UVoxelChunk* InChunk, FIndex InAnchorChunkIndex, int32 InX, int32 InY, int32 InBuildingIndex)
{
	if(!InChunk || !_PrefabCaches.IsValidIndex(InBuildingIndex)) return false;
	const FVoxelBuildingPlacementPlan Plan = GetOrBuildPlacementPlan(InAnchorChunkIndex, InX, InY, InBuildingIndex);
	if(!Plan.bValid || !_PrefabCaches[InBuildingIndex].Rotations.IsValidIndex(Plan.Rotation)) return false;
	const FVoxelBuildingPrefabCache& PrefabCache = _PrefabCaches[InBuildingIndex];
	const FVoxelBuildingRotationCache& RotationCache = PrefabCache.Rotations[Plan.Rotation];
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	auto SetSliceVoxel = [InChunk, ChunkOrigin, ChunkSize](const FIndex& WorldIndex, const FVoxelItem& VoxelItem, bool bSafe = false)
	{
		if(WorldIndex.X < ChunkOrigin.X || WorldIndex.X >= ChunkOrigin.X + ChunkSize.X ||
			WorldIndex.Y < ChunkOrigin.Y || WorldIndex.Y >= ChunkOrigin.Y + ChunkSize.Y) return;
		InChunk->SetVoxel(FIndex(WorldIndex.X - ChunkOrigin.X, WorldIndex.Y - ChunkOrigin.Y, WorldIndex.Z), VoxelItem, bSafe);
	};

	const int32 SliceMinX = FMath::Max(RotationCache.MinX, ChunkOrigin.X - InX);
	const int32 SliceMaxX = FMath::Min(RotationCache.MaxX, ChunkOrigin.X + ChunkSize.X - 1 - InX);
	const int32 SliceMinY = FMath::Max(RotationCache.MinY, ChunkOrigin.Y - InY);
	const int32 SliceMaxY = FMath::Min(RotationCache.MaxY, ChunkOrigin.Y + ChunkSize.Y - 1 - InY);
	for(int32 X = SliceMinX; X <= SliceMaxX; ++X)
	{
		for(int32 Y = SliceMinY; Y <= SliceMaxY; ++Y)
		{
			const int32 SurfaceHeight = Module->SampleTopographyByIndex(FIndex(InX + X, InY + Y)).Height;
			for(int32 Z = SurfaceHeight; Z <= Plan.GroundHeight; ++Z)
			{
				SetSliceVoxel(FIndex(InX + X, InY + Y, Z), EVoxelType::Cobble_Stone);
			}
		}
	}

	const int32 ClearMinX = FMath::Max(RotationCache.MinX - 1, ChunkOrigin.X - InX);
	const int32 ClearMaxX = FMath::Min(RotationCache.MaxX + 1, ChunkOrigin.X + ChunkSize.X - 1 - InX);
	const int32 ClearMinY = FMath::Max(RotationCache.MinY - 1, ChunkOrigin.Y - InY);
	const int32 ClearMaxY = FMath::Min(RotationCache.MaxY + 1, ChunkOrigin.Y + ChunkSize.Y - 1 - InY);
	for(int32 X = ClearMinX; X <= ClearMaxX; ++X)
	{
		for(int32 Y = ClearMinY; Y <= ClearMaxY; ++Y)
		{
			for(int32 Z = 1; Z <= PrefabCache.ClearHeight; ++Z)
			{
				SetSliceVoxel(FIndex(InX + X, InY + Y, Plan.GroundHeight + Z), FVoxelItem::Empty, true);
			}
		}
	}

	const FIndex BuildingOrigin(InX, InY, Plan.GroundHeight);
	const FIndex SliceIndex = InChunk->GetIndex() - InAnchorChunkIndex;
	if(const TArray<FVoxelItem>* SliceItems = RotationCache.ChunkSlices.Find(SliceIndex))
	{
		for(const FVoxelItem& VoxelItem : *SliceItems) SetSliceVoxel(BuildingOrigin + VoxelItem.Index, VoxelItem);
	}
	return true;
}
