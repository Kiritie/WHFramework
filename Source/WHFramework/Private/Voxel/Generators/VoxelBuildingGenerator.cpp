#include "Voxel/Generators/VoxelBuildingGenerator.h"

#include "Asset/AssetModuleStatics.h"
#include "Common/CommonModuleStatics.h"
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

}

void UVoxelBuildingGenerator::Initialize(UVoxelModule* InModule, int32 InStage)
{
	Super::Initialize(InModule, InStage);
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
			const bool bUnderground = Prefab->CenterOffset.Z < 0.f;
			const int32 VerticalOffset = bUnderground ? FMath::FloorToInt(Prefab->CenterOffset.Z) : 0;
			const FIndex CenterOffset(
				FMath::FloorToInt(Prefab->CenterOffset.X),
				FMath::FloorToInt(Prefab->CenterOffset.Y),
				0);
			TArray<FString> SerializedVoxelDatas;
			Prefab->VoxelDatas.ParseIntoArray(SerializedVoxelDatas, TEXT("|"));
			TArray<FVoxelItem> SourceItems;
			int32 SourceMinX = MAX_int32;
			int32 SourceMaxX = MIN_int32;
			int32 SourceMinY = MAX_int32;
			int32 SourceMaxY = MIN_int32;
			int32 SourceMinZ = MAX_int32;
			int32 EntranceBorderMinX = MAX_int32;
			int32 EntranceBorderMaxX = MIN_int32;
			int32 EntranceBorderMinY = MAX_int32;
			int32 EntranceBorderMaxY = MIN_int32;
			for(const FString& SerializedVoxelData : SerializedVoxelDatas)
			{
				FVoxelItem Item(SerializedVoxelData, true);
				if(!Item.IsValid()) continue;
				Item.Index = Item.Index - CenterOffset;
				SourceItems.Add(Item);
				SourceMinX = FMath::Min(SourceMinX, Item.Index.X);
				SourceMaxX = FMath::Max(SourceMaxX, Item.Index.X);
				SourceMinY = FMath::Min(SourceMinY, Item.Index.Y);
				SourceMaxY = FMath::Max(SourceMaxY, Item.Index.Y);
				SourceMinZ = FMath::Min(SourceMinZ, Item.Index.Z);
				if(bUnderground && Item.Index.Z + VerticalOffset == 0)
				{
					EntranceBorderMinX = FMath::Min(EntranceBorderMinX, Item.Index.X);
					EntranceBorderMaxX = FMath::Max(EntranceBorderMaxX, Item.Index.X);
					EntranceBorderMinY = FMath::Min(EntranceBorderMinY, Item.Index.Y);
					EntranceBorderMaxY = FMath::Max(EntranceBorderMaxY, Item.Index.Y);
				}
			}
			if(SourceItems.IsEmpty()) continue;

			TArray<FIndex> SourceEntranceIndices;
			if(bUnderground)
			{
				if(EntranceBorderMinX != MAX_int32 && EntranceBorderMinY != MAX_int32)
				{
					for(int32 X = EntranceBorderMinX + 1; X < EntranceBorderMaxX; ++X)
					{
						for(int32 Y = EntranceBorderMinY + 1; Y < EntranceBorderMaxY; ++Y)
						{
							SourceEntranceIndices.Emplace(X, Y, 0);
						}
					}
				}
			}
			else
			{
				for(int32 Y = SourceMinY; Y <= SourceMaxY; ++Y)
				{
					SourceEntranceIndices.Emplace(SourceMinX, Y, 0);
				}
			}

			FVoxelBuildingPrefabCache& PrefabCache = _PrefabCaches[DataIndex];
			PrefabCache.ClearHeight = bUnderground ? 0 : FMath::CeilToInt(Prefab->VoxelSize.Z);
			PrefabCache.GroundOffset = bUnderground ? VerticalOffset : 1 - SourceMinZ;
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
				RotationCache.MinZ = MAX_int32;
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
					RotationCache.MinZ = FMath::Min(RotationCache.MinZ, Item.Index.Z);
					RotationCache.MaxZ = FMath::Max(RotationCache.MaxZ, Item.Index.Z);
				}
				for(const FIndex& EntranceIndex : SourceEntranceIndices)
				{
					RotationCache.EntranceIndices.Add(FMathHelper::RotateIndex(EntranceIndex, RotationAngle) + RotationOffset);
				}
				if(!bUnderground)
				{
					PrefabCache.ClearHeight = FMath::Max(PrefabCache.ClearHeight, RotationCache.MaxZ + PrefabCache.GroundOffset);
				}
				const int32 RotationExtent = FMath::Max(
					FMath::Max(FMath::Abs(RotationCache.MinX), FMath::Abs(RotationCache.MaxX)),
					FMath::Max(FMath::Abs(RotationCache.MinY), FMath::Abs(RotationCache.MaxY)));
				PrefabCache.Extent = FMath::Max(PrefabCache.Extent, RotationExtent + 4);
			}
		}
	}
}

void UVoxelBuildingGenerator::GetPlacementGrid(int32 InBuildingIndex, FIndex& OutInterval, FIndex& OutOffset) const
{
	const auto& WorldData = Module->GetWorldData();
	const auto& Data = GenerateDatas[InBuildingIndex];
	const int32 Extent = _PrefabCaches[InBuildingIndex].Extent;
	OutInterval.X = FMath::Max(2, FMath::CeilToInt(FMath::Max(Data.SpawnInterval, float(Extent * 2 + WorldData.ChunkSize.X)) / (WorldData.ChunkSize.X * 2.f)) * 2);
	OutInterval.Y = FMath::Max(FMath::CeilToInt(OutInterval.X * WorldData.ChunkSize.X * FMath::Sqrt(3.f) * 0.5f / WorldData.ChunkSize.Y),
		FMath::DivideAndRoundUp(Extent * 2 + WorldData.ChunkSize.Y, WorldData.ChunkSize.Y));
	OutInterval.Z = 0;
	FRandomStream Random(HashCombineFast(GetTypeHash(WorldData.WorldSeed ^ Seed), FCrc::StrCrc32(*Data.PrefabAsset.ToString())));
	OutOffset = FIndex(Random.RandRange(0, OutInterval.X - 1), Random.RandRange(0, OutInterval.Y - 1), 0);
}

void UVoxelBuildingGenerator::Generate(UVoxelChunk* InChunk)
{
	if(!InChunk || !Module) return;
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	for(int32 BuildingIndex = 0; BuildingIndex < GenerateDatas.Num(); ++BuildingIndex)
	{
		if(!_PrefabAssets[BuildingIndex] || GenerateDatas[BuildingIndex].SpawnChance <= 0.f) continue;
		const int32 Extent = _PrefabCaches[BuildingIndex].Extent;
		FIndex Interval, Offset;
		GetPlacementGrid(BuildingIndex, Interval, Offset);
		const FIndex Margin(FMath::DivideAndRoundUp(Extent, ChunkSize.X), FMath::DivideAndRoundUp(Extent, ChunkSize.Y), 0);
		const FIndex ChunkIndex = InChunk->GetIndex();
		for(int32 Y = FMath::CeilToInt(double(ChunkIndex.Y - Margin.Y - Offset.Y) / Interval.Y); Y <= FMath::FloorToInt(double(ChunkIndex.Y + Margin.Y - Offset.Y) / Interval.Y); ++Y)
		{
			const int32 RowOffset = Offset.X + (Y & 1) * (Interval.X / 2);
			for(int32 X = FMath::CeilToInt(double(ChunkIndex.X - Margin.X - RowOffset) / Interval.X); X <= FMath::FloorToInt(double(ChunkIndex.X + Margin.X - RowOffset) / Interval.X); ++X)
			{
				const FIndex CellIndex(X, Y, 0);
				const FVoxelBuildingPlacementPlan Plan = GetOrBuildPlacementPlan(CellIndex, BuildingIndex);
				if(!Plan.bValid || !PlaceBuildingSlice(InChunk, BuildingIndex, Plan)) continue;
				if(Plan.AnchorChunkIndex == InChunk->GetIndex())
				{
					const FIndex Center = Module->ChunkIndexToVoxelIndex(Plan.AnchorChunkIndex) + FIndex(ChunkSize.X / 2, ChunkSize.Y / 2, 0);
					const auto& Rotation = _PrefabCaches[BuildingIndex].Rotations[Plan.Rotation];
					FSceneArea Area;
					Area.AreaName = *FString::Printf(TEXT("Structure_%d_%d_%d"), BuildingIndex, CellIndex.X, CellIndex.Y);
					const FText DisplayName = _PrefabAssets[BuildingIndex]->DisplayName.IsEmpty()
						? UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EVoxelRegionType"), static_cast<int32>(EVoxelRegionType::Building))
						: _PrefabAssets[BuildingIndex]->DisplayName;
					Area.AreaDisplayName = Module->GetVoxelAreaName(FIndex(Center.X, Center.Y, Plan.GroundHeight), EVoxelAreaType::Building, DisplayName);
					Area.AreaShape = ESceneAreaShape::Box;
					Area.AreaCenter = FVector2D(Center.X + (Rotation.MinX + Rotation.MaxX) * 0.5f, Center.Y + (Rotation.MinY + Rotation.MaxY) * 0.5f);
					Area.AreaRadius = FVector2D((Rotation.MaxX - Rotation.MinX) * 0.5f + 4.f, (Rotation.MaxY - Rotation.MinY) * 0.5f + 4.f);
					USceneModuleStatics::AddSceneArea(Area, true);
				}
			}
		}
	}
}

FVoxelBuildingPlacementPlan UVoxelBuildingGenerator::BuildPlacementPlan(int32 InX, int32 InY, int32 InBuildingIndex, bool bInAdaptTerrain) const
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
				if(!bInAdaptTerrain && (Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.BiomeType == EVoxelBiomeType::River || Topography.RegionType == EVoxelRegionType::Ocean || Topography.RegionType == EVoxelRegionType::River || Topography.RegionType == EVoxelRegionType::Lake))
				{
					bInvalidTerrain = true;
					break;
				}
				if(X >= RotationCache.MinX && X <= RotationCache.MaxX && Y >= RotationCache.MinY && Y <= RotationCache.MaxY)
				{
					const int32 Height = SampleHeight(SampleIndex);
					AverageHeight += Height;
					MinHeight = FMath::Min(MinHeight, Height);
					MaxHeight = FMath::Max(MaxHeight, FMath::Max(Height, Topography.WaterHeight));
					++FootprintCount;
				}
			}
			if(bInvalidTerrain) break;
		}
		if(bInvalidTerrain || FootprintCount <= 0 || (!bInAdaptTerrain && MaxHeight - MinHeight > GenerateData.MaxTerrainSlope)) continue;
		const int32 GroundHeight = bInAdaptTerrain
			? FMath::Min(FMath::Max(MaxHeight, Module->GetWorldData().SeaLevel + GenerateData.MinHeightAboveSeaLevel), Module->GetWorldData().SkyHeight - PrefabCache.ClearHeight - 2)
			: FMath::RoundToInt(AverageHeight / FootprintCount);
		if(GroundHeight < Module->GetWorldData().SeaLevel + GenerateData.MinHeightAboveSeaLevel || GroundHeight + RotationCache.MinZ + PrefabCache.GroundOffset < 1 || GroundHeight + RotationCache.MaxZ + PrefabCache.GroundOffset >= Module->GetWorldData().SkyHeight) continue;

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

FVoxelBuildingPlacementPlan UVoxelBuildingGenerator::GetOrBuildPlacementPlan(FIndex InCellIndex, int32 InBuildingIndex)
{
	const auto& WorldData = Module->GetWorldData();
	const FIntVector4 Key(InCellIndex.X, InCellIndex.Y, InBuildingIndex, WorldData.WorldSeed);
	{
		FReadScopeLock ReadLock(_BuildingPlanCacheLock);
		if(const auto* Cached = _BuildingPlanCache.Find(Key)) return *Cached;
	}
	FVoxelBuildingPlacementPlan Plan;
	const auto& Data = GenerateDatas[InBuildingIndex];
	const auto& Cache = _PrefabCaches[InBuildingIndex];
	const FIndex ChunkSize = WorldData.ChunkSize;
	FIndex Interval, Offset;
	GetPlacementGrid(InBuildingIndex, Interval, Offset);
	FRandomStream Random(HashCombineFast(GetTypeHash(Key), FCrc::StrCrc32(*Data.PrefabAsset.ToString())) ^ Seed);
	if(Random.FRand() < Data.SpawnChance && Cache.Rotations.Num() == 4)
	{
		const FIndex Anchor(InCellIndex.X * Interval.X + Offset.X + (InCellIndex.Y & 1) * (Interval.X / 2), InCellIndex.Y * Interval.Y + Offset.Y, 0);
		const FIndex Center = Module->ChunkIndexToVoxelIndex(Anchor) + FIndex(ChunkSize.X / 2, ChunkSize.Y / 2, 0);
		bool bOverlaps = false;
		for(int32 Other = 0; Other < InBuildingIndex && !bOverlaps; ++Other)
		{
			if(!_PrefabAssets[Other] || GenerateDatas[Other].SpawnChance <= 0.f) continue;
			const auto& OtherCache = _PrefabCaches[Other];
			FIndex OtherInterval, OtherOffset;
			GetPlacementGrid(Other, OtherInterval, OtherOffset);
			const FIndex Margin(FMath::DivideAndRoundUp(Cache.Extent + OtherCache.Extent, ChunkSize.X), FMath::DivideAndRoundUp(Cache.Extent + OtherCache.Extent, ChunkSize.Y), 0);
			for(int32 Y = FMath::CeilToInt(double(Anchor.Y - Margin.Y - OtherOffset.Y) / OtherInterval.Y); Y <= FMath::FloorToInt(double(Anchor.Y + Margin.Y - OtherOffset.Y) / OtherInterval.Y) && !bOverlaps; ++Y)
			{
				const int32 RowOffset = OtherOffset.X + (Y & 1) * (OtherInterval.X / 2);
				for(int32 X = FMath::CeilToInt(double(Anchor.X - Margin.X - RowOffset) / OtherInterval.X); X <= FMath::FloorToInt(double(Anchor.X + Margin.X - RowOffset) / OtherInterval.X) && !bOverlaps; ++X)
				{
					const auto OtherPlan = GetOrBuildPlacementPlan(FIndex(X, Y, 0), Other);
					if(!OtherPlan.bValid) continue;
					const FIndex OtherCenter = Module->ChunkIndexToVoxelIndex(OtherPlan.AnchorChunkIndex) + FIndex(ChunkSize.X / 2, ChunkSize.Y / 2, 0);
					bOverlaps = FMath::Abs(Center.X - OtherCenter.X) < Cache.Extent + OtherCache.Extent && FMath::Abs(Center.Y - OtherCenter.Y) < Cache.Extent + OtherCache.Extent;
				}
			}
		}
		if(!bOverlaps)
		{
			Plan = BuildPlacementPlan(Center.X, Center.Y, InBuildingIndex, false);
			if(!Plan.bValid && Data.bAllowTerrainAdaptation) Plan = BuildPlacementPlan(Center.X, Center.Y, InBuildingIndex, true);
			Plan.AnchorChunkIndex = Anchor;
		}
	}
	FWriteScopeLock WriteLock(_BuildingPlanCacheLock);
	if(const auto* Cached = _BuildingPlanCache.Find(Key)) return *Cached;
	_BuildingPlanCache.Add(Key, Plan);
	_BuildingPlanCacheOrder.Add(Key);
	while(_BuildingPlanCacheOrder.Num() > 128)
	{
		_BuildingPlanCache.Remove(_BuildingPlanCacheOrder[0]);
		_BuildingPlanCacheOrder.RemoveAt(0);
	}
	return Plan;
}

bool UVoxelBuildingGenerator::PlaceBuildingSlice(UVoxelChunk* InChunk, int32 InBuildingIndex, const FVoxelBuildingPlacementPlan& Plan)
{
	if(!InChunk || !_PrefabCaches.IsValidIndex(InBuildingIndex)) return false;
	if(!Plan.bValid || !_PrefabCaches[InBuildingIndex].Rotations.IsValidIndex(Plan.Rotation)) return false;
	const FVoxelBuildingPrefabCache& PrefabCache = _PrefabCaches[InBuildingIndex];
	const FVoxelBuildingRotationCache& RotationCache = PrefabCache.Rotations[Plan.Rotation];
	const bool bUnderground = _PrefabAssets.IsValidIndex(InBuildingIndex)
		&& IsValid(_PrefabAssets[InBuildingIndex])
		&& _PrefabAssets[InBuildingIndex]->CenterOffset.Z < 0.f;
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex Center = Module->ChunkIndexToVoxelIndex(Plan.AnchorChunkIndex) + FIndex(ChunkSize.X / 2, ChunkSize.Y / 2, 0);
	const int32 InX = Center.X, InY = Center.Y;
	if(ChunkOrigin.X > InX + RotationCache.MaxX + 4 || ChunkOrigin.X + ChunkSize.X <= InX + RotationCache.MinX - 4 ||
		ChunkOrigin.Y > InY + RotationCache.MaxY + 4 || ChunkOrigin.Y + ChunkSize.Y <= InY + RotationCache.MinY - 4) return false;
	auto SetSliceVoxel = [InChunk, ChunkOrigin, ChunkSize](const FIndex& WorldIndex, const FVoxelItem& VoxelItem, bool bSafe = false)
	{
		if(WorldIndex.X < ChunkOrigin.X || WorldIndex.X >= ChunkOrigin.X + ChunkSize.X ||
			WorldIndex.Y < ChunkOrigin.Y || WorldIndex.Y >= ChunkOrigin.Y + ChunkSize.Y) return;
		InChunk->SetVoxel(FIndex(WorldIndex.X - ChunkOrigin.X, WorldIndex.Y - ChunkOrigin.Y, WorldIndex.Z), VoxelItem, bSafe);
	};

	const TSet<FIndex> EntranceColumns(RotationCache.EntranceIndices);
	auto IsClearedRoot = [&](FIndex WorldIndex)
	{
		const FIndex RelativeIndex(WorldIndex.X - InX, WorldIndex.Y - InY, 0);
		if(bUnderground)
		{
			return EntranceColumns.Contains(RelativeIndex) ||
				WorldIndex.Z <= Plan.GroundHeight && WorldIndex.Z > Plan.GroundHeight + RotationCache.MinZ + PrefabCache.GroundOffset &&
				RelativeIndex.X >= RotationCache.MinX && RelativeIndex.X <= RotationCache.MaxX &&
				RelativeIndex.Y >= RotationCache.MinY && RelativeIndex.Y <= RotationCache.MaxY;
		}
		return RelativeIndex.X >= RotationCache.MinX - 1 && RelativeIndex.X <= RotationCache.MaxX + 1 &&
			RelativeIndex.Y >= RotationCache.MinY - 1 && RelativeIndex.Y <= RotationCache.MaxY + 1;
	};
	for(int32 X = FMath::Max(InX + RotationCache.MinX - 4, ChunkOrigin.X); X <= FMath::Min(InX + RotationCache.MaxX + 4, ChunkOrigin.X + ChunkSize.X - 1); ++X)
	{
		for(int32 Y = FMath::Max(InY + RotationCache.MinY - 4, ChunkOrigin.Y); Y <= FMath::Min(InY + RotationCache.MaxY + 4, ChunkOrigin.Y + ChunkSize.Y - 1); ++Y)
		{
			for(int32 Z = 1; Z < Module->GetWorldData().SkyHeight; ++Z)
			{
				const FIndex WorldIndex(X, Y, Z);
				const FIndex LocalIndex(X - ChunkOrigin.X, Y - ChunkOrigin.Y, Z);
				const FVoxelItem& Item = InChunk->GetVoxel(LocalIndex);
				if(!Item.IsValid()) continue;
				const bool bTreePart = Item.Data == TEXT("R") || Item.Data.StartsWith(TEXT("T"));
				if(!bTreePart && Item.GetData().Nature != EVoxelNature::Foliage && Item.GetData().Nature != EVoxelNature::SemiFoliage) continue;
				const FIndex RootIndex = Item.Data.StartsWith(TEXT("T")) ? WorldIndex + FIndex(Item.Data.Mid(1)) : WorldIndex;
				if(IsClearedRoot(RootIndex)) InChunk->SetVoxel(LocalIndex, FVoxelItem::Empty, true);
			}
		}
	}

	const int32 SliceMinX = FMath::Max(RotationCache.MinX, ChunkOrigin.X - InX);
	const int32 SliceMaxX = FMath::Min(RotationCache.MaxX, ChunkOrigin.X + ChunkSize.X - 1 - InX);
	const int32 SliceMinY = FMath::Max(RotationCache.MinY, ChunkOrigin.Y - InY);
	const int32 SliceMaxY = FMath::Min(RotationCache.MaxY, ChunkOrigin.Y + ChunkSize.Y - 1 - InY);
	if(bUnderground)
	{
		for(int32 X = SliceMinX; X <= SliceMaxX; ++X)
		{
			for(int32 Y = SliceMinY; Y <= SliceMaxY; ++Y)
			{
				for(int32 Z = RotationCache.MinZ + PrefabCache.GroundOffset; Z < 0; ++Z)
				{
					SetSliceVoxel(FIndex(InX + X, InY + Y, Plan.GroundHeight + Z), FVoxelItem::Empty, true);
				}
			}
		}
		for(const FIndex& EntranceIndex : RotationCache.EntranceIndices)
		{
			const FIndex WorldIndex(InX + EntranceIndex.X, InY + EntranceIndex.Y, 0);
			if(WorldIndex.X < ChunkOrigin.X || WorldIndex.X >= ChunkOrigin.X + ChunkSize.X ||
				WorldIndex.Y < ChunkOrigin.Y || WorldIndex.Y >= ChunkOrigin.Y + ChunkSize.Y) continue;
			const int32 SurfaceHeight = InChunk->GetTopography(FIndex(WorldIndex.X - ChunkOrigin.X, WorldIndex.Y - ChunkOrigin.Y, 0)).Height;
			for(int32 Z = Plan.GroundHeight; Z <= FMath::Max(Plan.GroundHeight, SurfaceHeight); ++Z)
			{
				SetSliceVoxel(FIndex(WorldIndex.X, WorldIndex.Y, Z), FVoxelItem::Empty, true);
			}
		}
	}
	else
	{
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
				const int32 ClearHeight = FMath::Max(PrefabCache.ClearHeight, Module->SampleTopographyByIndex(FIndex(InX + X, InY + Y)).Height - Plan.GroundHeight);
				for(int32 Z = 1; Z <= ClearHeight; ++Z)
				{
					SetSliceVoxel(FIndex(InX + X, InY + Y, Plan.GroundHeight + Z), FVoxelItem::Empty, true);
				}
			}
		}
	}

	for(int32 X = FMath::Max(RotationCache.MinX - 4, ChunkOrigin.X - InX); X <= FMath::Min(RotationCache.MaxX + 4, ChunkOrigin.X + ChunkSize.X - 1 - InX); ++X)
	{
		for(int32 Y = FMath::Max(RotationCache.MinY - 4, ChunkOrigin.Y - InY); Y <= FMath::Min(RotationCache.MaxY + 4, ChunkOrigin.Y + ChunkSize.Y - 1 - InY); ++Y)
		{
			FVoxelTopography& Topography = InChunk->GetTopography(FIndex(InX + X - ChunkOrigin.X, InY + Y - ChunkOrigin.Y, 0));
			Topography.RegionType = EVoxelRegionType::Building;
			if(!bUnderground && X >= RotationCache.MinX && X <= RotationCache.MaxX && Y >= RotationCache.MinY && Y <= RotationCache.MaxY)
			{
				Topography.Height = Plan.GroundHeight;
				Topography.WaterHeight = 0;
			}
		}
	}

	const FIndex BuildingOrigin(InX, InY, Plan.GroundHeight);
	const FIndex SliceIndex = InChunk->GetIndex() - Plan.AnchorChunkIndex;
	if(const TArray<FVoxelItem>* SliceItems = RotationCache.ChunkSlices.Find(SliceIndex))
	{
		for(const FVoxelItem& VoxelItem : *SliceItems) SetSliceVoxel(BuildingOrigin + VoxelItem.Index + FIndex(0, 0, PrefabCache.GroundOffset), VoxelItem);
	}
	return true;
}
