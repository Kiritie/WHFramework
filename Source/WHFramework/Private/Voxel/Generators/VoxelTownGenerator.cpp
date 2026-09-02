
#include "Voxel/Generators/VoxelTownGenerator.h"

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

namespace
{
	struct FTownDomainPoint
	{
		float Cost;
		FVector2D Position;
	};

	struct FTownDomainPointCompare
	{
		bool operator()(const FTownDomainPoint& A, const FTownDomainPoint& B) const
		{
			return A.Cost > B.Cost;
		}
	};
}

UVoxelTownGenerator::UVoxelTownGenerator()
{
	Seed = 453;
	SpawnRate = 0.001f;
	InfluenceRadius = 64;

	PathFinder.SetConditionInBarrier([this](FVector2D Pos) { return InBarrier(Pos); });
	PathFinder.SetWeightFormula([this](FVector2D StartPos, FVector2D EndPos, float Cost) { return WeightFormula(StartPos, EndPos, Cost); });

	PrefabAssets = TArray<FPrimaryAssetId>();
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_House_1")));
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_House_2")));
	PrefabAssets.Add(FPrimaryAssetId(TEXT("VoxelPrefab:DA_House_3")));

	_StartPoint = FVector2D::ZeroVector;
}

void UVoxelTownGenerator::Initialize(UVoxelModule* InModule, int32 InStage)
{
	Super::Initialize(InModule, InStage);

	for(const FPrimaryAssetId& PrefabAsset : PrefabAssets)
	{
		if(UVoxelPrefabData* Prefab = UAssetModuleStatics::LoadPrimaryAsset<UVoxelPrefabData>(PrefabAsset))
		{
			_PrefabAssets.Add(Prefab);
			TArray<FString> VoxelDatas;
			Prefab->VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
			int32 MinZ = MAX_int32;
			for(const FString& VoxelData : VoxelDatas)
			{
				const FVoxelItem VoxelItem(VoxelData, true);
				if(VoxelItem.IsValid()) MinZ = FMath::Min(MinZ, VoxelItem.Index.Z);
			}
			_PrefabGroundOffsets.Add(MinZ == MAX_int32 ? 0 : 1 - MinZ);
		}
	}
}

void UVoxelTownGenerator::Generate(UVoxelChunk* InChunk)
{
	if(!InChunk || !Module || _PrefabAssets.IsEmpty()) return;
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 MarginX = FMath::CeilToInt(static_cast<float>(InfluenceRadius) / FMath::Max(ChunkSize.X, 1)) + 1;
	const int32 MarginY = FMath::CeilToInt(static_cast<float>(InfluenceRadius) / FMath::Max(ChunkSize.Y, 1)) + 1;
	for(int32 AnchorX = InChunk->GetIndex().X - MarginX; AnchorX <= InChunk->GetIndex().X + MarginX; ++AnchorX)
	{
		for(int32 AnchorY = InChunk->GetIndex().Y - MarginY; AnchorY <= InChunk->GetIndex().Y + MarginY; ++AnchorY)
		{
			const FIndex AnchorChunkIndex(AnchorX, AnchorY, 0);
			if((1.f - FMathHelper::HashRand(AnchorChunkIndex.ToVector2D(), Seed)) > SpawnRate) continue;
			const FIndex AnchorOrigin = Module->ChunkIndexToVoxelIndex(AnchorChunkIndex);
			const FVoxelTopography AnchorTopography = Module->SampleTopographyByIndex(FIndex(AnchorOrigin.X + ChunkSize.X / 2, AnchorOrigin.Y + ChunkSize.Y / 2, 0));
			if(AnchorTopography.Height <= Module->GetWorldData().SeaLevel + 1 || AnchorTopography.RegionType == EVoxelRegionType::Mountain ||
				AnchorTopography.RegionType == EVoxelRegionType::River || AnchorTopography.RegionType == EVoxelRegionType::Lake || AnchorTopography.RegionType == EVoxelRegionType::Ocean || AnchorTopography.BiomeType == EVoxelBiomeType::Ocean || AnchorTopography.BiomeType == EVoxelBiomeType::River) continue;
			TSharedPtr<TMap<FIndex, FVoxelItem>> Plan;
			{
				FReadScopeLock ReadLock(_TownPlanCacheLock);
				if(const TSharedPtr<TMap<FIndex, FVoxelItem>>* CachedPlan = _TownPlanCache.Find(AnchorChunkIndex)) Plan = *CachedPlan;
			}
			if(!Plan)
			{
				FScopeLock PlanningLock(&CriticalSection);
				{
					FReadScopeLock ReadLock(_TownPlanCacheLock);
					if(const TSharedPtr<TMap<FIndex, FVoxelItem>>* CachedPlan = _TownPlanCache.Find(AnchorChunkIndex)) Plan = *CachedPlan;
				}
				if(!Plan)
				{
					PlanTown(AnchorChunkIndex);
					Plan = MakeShared<TMap<FIndex, FVoxelItem>>(MoveTemp(_PlannedVoxels));
					FWriteScopeLock WriteLock(_TownPlanCacheLock);
					_TownPlanCache.Add(AnchorChunkIndex, Plan);
					_TownPlanCacheOrder.Add(AnchorChunkIndex);
					while(_TownPlanCacheOrder.Num() > 64)
					{
						_TownPlanCache.Remove(_TownPlanCacheOrder[0]);
						_TownPlanCacheOrder.RemoveAt(0);
					}
				}
			}
			ApplyTownSlice(InChunk, *Plan);
			if(AnchorChunkIndex == InChunk->GetIndex() && !Plan->IsEmpty())
			{
				FBox2D TownBounds(ForceInit);
				for(const auto& Iter : *Plan)
				{
					TownBounds += Iter.Key.ToVector2D();
				}
				FSceneArea SceneArea;
				SceneArea.AreaName = *FString::Printf(TEXT("Town_%d_%d"), AnchorChunkIndex.X, AnchorChunkIndex.Y);
				SceneArea.AreaDisplayName = Module->GetVoxelAreaName(AnchorOrigin, EVoxelAreaType::Town,
					UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EVoxelRegionType"), static_cast<int32>(EVoxelRegionType::Town)));
				SceneArea.AreaType = ESceneAreaType::Default;
				SceneArea.AreaShape = ESceneAreaShape::Box;
				SceneArea.AreaCenter = TownBounds.GetCenter();
				SceneArea.AreaRadius = TownBounds.GetExtent() + FVector2D(6.f);
				USceneModuleStatics::AddSceneArea(SceneArea, true);
			}
		}
	}
}

void UVoxelTownGenerator::PlanTown(FIndex InAnchorChunkIndex)
{
	_Domains.Reset();
	_Roads.Reset();
	_BuildingPos.Reset();
	_PlannedVoxels.Reset();
	_TopographyHeightCache.Reset();
	DevelopeDomains(InAnchorChunkIndex);
	PlaceBuildings(InAnchorChunkIndex);
	PlacePaths();
}

void UVoxelTownGenerator::ApplyTownSlice(UVoxelChunk* InChunk, const TMap<FIndex, FVoxelItem>& InPlan) const
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	for(const auto& Iter : InPlan)
	{
		const FIndex& WorldIndex = Iter.Key;
		if(WorldIndex.X < ChunkOrigin.X || WorldIndex.X >= ChunkOrigin.X + ChunkSize.X ||
			WorldIndex.Y < ChunkOrigin.Y || WorldIndex.Y >= ChunkOrigin.Y + ChunkSize.Y) continue;
		InChunk->SetVoxel(FIndex(WorldIndex.X - ChunkOrigin.X, WorldIndex.Y - ChunkOrigin.Y, WorldIndex.Z), Iter.Value, !Iter.Value.IsValid());
	}
}

void UVoxelTownGenerator::SetPlannedVoxel(FIndex InWorldIndex, const FVoxelItem& InVoxelItem)
{
	if(FVector2D::Distance(InWorldIndex.ToVector2D(), _StartPoint) > InfluenceRadius) return;
	_PlannedVoxels.Add(InWorldIndex, InVoxelItem);
}

int32 UVoxelTownGenerator::SamplePlannedHeight(FIndex InWorldIndex)
{
	InWorldIndex.Z = 0;
	if(const int32* Height = _TopographyHeightCache.Find(InWorldIndex)) return *Height;
	const int32 Height = Module->SampleTopographyByIndex(InWorldIndex).Height;
	_TopographyHeightCache.Add(InWorldIndex, Height);
	return Height;
}

void UVoxelTownGenerator::DevelopeDomains(FIndex InAnchorChunkIndex)
{
	const FIndex AnchorOrigin = Module->ChunkIndexToVoxelIndex(InAnchorChunkIndex);
	_StartPoint = FVector2D(AnchorOrigin.X + 7, AnchorOrigin.Y + 7);

	std::priority_queue<FTownDomainPoint, std::vector<FTownDomainPoint>, FTownDomainPointCompare> Points;
	Points.push({ 0.f, _StartPoint });

	int32 Count = 0;
	const int32 Dx[9] = {1, -1, 0, 0, 1, -1, 1, -1, 0};
	const int32 Dy[9] = {0, 0, 1, -1, 1, -1, -1, 1, 0};

	while(!Points.empty())
	{
		Count++;
		float Cost = Points.top().Cost;
		FVector2D P = Points.top().Position;
		Points.pop();

		for(int d = 0; d < 9; ++d)
		{
			_Domains.Emplace(FMathHelper::CompressIndex(P.X + Dx[d], P.Y + Dy[d]));
		}

		if(Cost > 7) break;

		const int32 CenterHeight = SamplePlannedHeight(FIndex(P.X, P.Y));

		const int32 Dx1[4] = {1, -1, 0, 0};
		const int32 Dy1[4] = {0, 0, 1, -1};

		for(int d = 0; d < 4; ++d)
		{
			int32 x = P.X + Dx1[d] * 3;
			int32 y = P.Y + Dy1[d] * 3;

			if(_Domains.Find(FMathHelper::CompressIndex(x, y))) continue;

			const FVoxelTopography Topography = Module->SampleTopographyByIndex(FIndex(x, y, 0));
			int32 Height = SamplePlannedHeight(FIndex(x, y));
			if(Height <= Module->GetWorldData().SeaLevel || Topography.RegionType == EVoxelRegionType::River || Topography.RegionType == EVoxelRegionType::Lake || Topography.RegionType == EVoxelRegionType::Ocean || Topography.BiomeType == EVoxelBiomeType::River || Topography.BiomeType == EVoxelBiomeType::Ocean) continue;

			int32 DeltaHeight = FMath::Abs(CenterHeight - Height);

			Points.push({ Cost + 0.5f + DeltaHeight * 0.75f, FVector2D(x, y) });
		}
	}
}

void UVoxelTownGenerator::PlaceBuildings(FIndex InAnchorChunkIndex)
{
	const int32 Dx[4] = {1, -1, 0, 0};
	const int32 Dy[4] = {0, 0, 1, -1};

	int32 Count = 0;

	std::queue<FVector2D> Points;
	Points.push(_StartPoint);

	while(!Points.empty())
	{
		++Count;

		const auto Pos = Points.front();
		Points.pop();

		const int32 Index = FMathHelper::HashRandInt(InAnchorChunkIndex.ToVector2D() + FVector2D(Count, -Count) * 107, Seed) % _PrefabAssets.Num();
		const int32 Rotate = FMathHelper::HashRandInt(InAnchorChunkIndex.ToVector2D() + FVector2D(Count, -Count) * 17, Seed) % 4;

		if(!PlaceOneBuilding(Pos.X, Pos.Y, Index, Rotate) || PlaceOneBuilding(Pos.X, Pos.Y, Index, ((Rotate + 1) % 4)))
		{
			continue;
		}

		const int32 Offset = FMathHelper::HashRandInt(InAnchorChunkIndex.ToVector2D() + FVector2D(Count, -Count) * 67, Seed) % 3 + 5;
		const int32 OffsetX = FMathHelper::HashRandInt(InAnchorChunkIndex.ToVector2D() + FVector2D(Count, Count) * 61, Seed) % 5 - 2;
		const int32 OffsetY = FMathHelper::HashRandInt(InAnchorChunkIndex.ToVector2D() + FVector2D(-Count, Count) * 117, Seed) % 5 - 2;

		for(int i = 0; i < 4; ++i)
		{
			Points.push(FVector2D(Pos.X + Dx[i] * (Offset + _PrefabAssets[Index]->VoxelSize.X) + OffsetX, Pos.Y + Dy[i] * (Offset + _PrefabAssets[Index]->VoxelSize.Y) + OffsetY));
		}
	}
}

bool UVoxelTownGenerator::PlaceOneBuilding(int32 InX, int32 InY, int32 InIndex, int32 InRotate)
{
	const int RotateIndex = InRotate % 2;
	const int FrontBack = _PrefabAssets[InIndex]->VoxelSize[RotateIndex] / 2;
	const int LeftRight = _PrefabAssets[InIndex]->VoxelSize[!RotateIndex] / 2;
	const int UpDown = _PrefabAssets[InIndex]->VoxelSize[2];
	const int32 GroundOffset = _PrefabGroundOffsets.IsValidIndex(InIndex) ? _PrefabGroundOffsets[InIndex] : 0;

	float Aver = 0;
	for(int i = -FrontBack - 1; i <= FrontBack; ++i)
	{
		for(int j = -LeftRight - 1; j <= LeftRight; ++j)
		{
			const FVoxelTopography Topography = Module->SampleTopographyByIndex(FIndex(InX + i, InY + j, 0));
			if(Topography.RegionType == EVoxelRegionType::Ocean || Topography.RegionType == EVoxelRegionType::River ||
				Topography.RegionType == EVoxelRegionType::Lake || Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.BiomeType == EVoxelBiomeType::River) return false;
		}
	}
	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			if(!_Domains.Find(FMathHelper::CompressIndex(InX + i, InY + j))) return false;

			Aver += SamplePlannedHeight(FIndex(InX + i, InY + j));
		}
	}

	Aver /= _PrefabAssets[InIndex]->VoxelSize[0] * _PrefabAssets[InIndex]->VoxelSize[1];
	Aver = floor(Aver + 0.5f);

	if(Aver <= Module->GetWorldData().SeaLevel) return false;

	for(int i = -FrontBack; i < FrontBack; ++i)
	{
		for(int j = -LeftRight; j < LeftRight; ++j)
		{
			for(int k = SamplePlannedHeight(FIndex(InX + i, InY + j)); k <= Aver; ++k)
			{
				const FIndex Index = FIndex(InX + i, InY + j, k);
				SetPlannedVoxel(Index, EVoxelType::Cobble_Stone);
			}
			_Domains.Remove(FMathHelper::CompressIndex(InX + i, InY + j));
		}
	}

	for(int i = -FrontBack - 1; i < FrontBack + 1; ++i)
	{
		for(int j = -LeftRight - 1; j < LeftRight + 1; ++j)
		{
			for(int k = 0; k < UpDown; ++k)
			{
				const FIndex Index = FIndex(InX + i, InY + j, Aver + k + 1);
				SetPlannedVoxel(Index, FVoxelItem::Empty);
			}
		}
	}

	TArray<FString> VoxelDatas;
	_PrefabAssets[InIndex]->VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
	for(auto& Iter : VoxelDatas)
	{
		FVoxelItem VoxelItem = FVoxelItem(Iter, true);
		VoxelItem.Index = VoxelItem.Index - FIndex(
			FMath::FloorToInt(_PrefabAssets[InIndex]->CenterOffset.X),
			FMath::FloorToInt(_PrefabAssets[InIndex]->CenterOffset.Y),
			0);
		if(VoxelItem.GetData().bRotatable)
		{
			VoxelItem.Angle = FMathHelper::CombineRightAngle(VoxelItem.Angle, (ERightAngle)InRotate);
		}
		const FIndex Index = FIndex(InX, InY, Aver + GroundOffset) + FMathHelper::RotateIndex(VoxelItem.Index, (ERightAngle)InRotate) + UVoxelModuleStatics::RightAngleToVoxelIndex((ERightAngle)InRotate);
		SetPlannedVoxel(Index, VoxelItem);
	}

	_Domains.Emplace(FMathHelper::CompressIndex(InX - FrontBack, InY - LeftRight));
	_BuildingPos.Push(FVector2D(InX - FrontBack, InY - LeftRight));

	const FText BuildingDisplayName = _PrefabAssets[InIndex]->DisplayName.IsEmpty()
		? UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EVoxelRegionType"), static_cast<int32>(EVoxelRegionType::Building))
		: _PrefabAssets[InIndex]->DisplayName;
	FSceneArea BuildingArea;
	BuildingArea.AreaName = *FString::Printf(TEXT("TownBuilding_%d_%d"), InX, InY);
	BuildingArea.AreaDisplayName = Module->GetVoxelAreaName(FIndex(InX, InY, Aver), EVoxelAreaType::Building, BuildingDisplayName);
	BuildingArea.AreaType = ESceneAreaType::Default;
	BuildingArea.AreaShape = ESceneAreaShape::Box;
	BuildingArea.AreaCenter = FVector2D(InX, InY);
	BuildingArea.AreaRadius = FVector2D(FrontBack + 1, LeftRight + 1);
	USceneModuleStatics::AddSceneArea(BuildingArea, true);

	return true;
}

void UVoxelTownGenerator::PlacePaths()
{
	for(int i = 1; i < _BuildingPos.Num(); ++i)
	{
		int32 NearestIndex = 0;
		double NearestDistance = FVector2D::DistSquared(_BuildingPos[i], _BuildingPos[0]);
		for(int j = 1; j < i; ++j)
		{
			const double Distance = FVector2D::DistSquared(_BuildingPos[i], _BuildingPos[j]);
			if(Distance < NearestDistance) { NearestDistance = Distance; NearestIndex = j; }
		}
		const auto Path = PathFinder.FindPath(_BuildingPos[i], _BuildingPos[NearestIndex]);
		for(FVector2D Pos : Path)
		{
			_Roads.Emplace(FMathHelper::CompressIndex(Pos.X, Pos.Y));
			const FIndex Index = FIndex(Pos.X, Pos.Y, SamplePlannedHeight(FIndex(Pos.X, Pos.Y)));
			SetPlannedVoxel(Index, EVoxelType::Cobble_Stone);
		}
	}
	_BuildingPos.Reset();
}

bool UVoxelTownGenerator::InBarrier(FVector2D InPos)
{
	if(!_Domains.Contains(FMathHelper::CompressIndex(InPos.X, InPos.Y))) return true;
	const FVoxelTopography Topography = Module->SampleTopographyByIndex(FIndex(InPos.X, InPos.Y, 0));
	if(Topography.BiomeType == EVoxelBiomeType::River || Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.RegionType == EVoxelRegionType::River || Topography.RegionType == EVoxelRegionType::Lake || Topography.RegionType == EVoxelRegionType::Ocean) return true;
	const int32 Height = Topography.Height;
	static const FIndex Offsets[] = { FIndex(1, 0, 0), FIndex(-1, 0, 0), FIndex(0, 1, 0), FIndex(0, -1, 0) };
	for(const FIndex& Offset : Offsets)
	{
		if(FMath::Abs(Module->SampleTopographyByIndex(FIndex(InPos.X + Offset.X, InPos.Y + Offset.Y, 0)).Height - Height) > 2) return true;
	}
	return false;
}

TPair<float, float> UVoxelTownGenerator::WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost)
{
	if(_Roads.Contains(FMathHelper::CompressIndex(InStartPos.X, InStartPos.Y)))
	{
		InCost -= 0.5f;
	}
	const FVoxelTopography Topography = Module->SampleTopographyByIndex(FIndex(InStartPos.X, InStartPos.Y, 0));
	InCost += FMath::Max(Topography.Erosion - 0.55f, 0.f) * 2.f;

	const FVector2D Dist =(InEndPos - InStartPos).GetAbs();
	float Predict =(Dist.X + Dist.Y) * 1.41f - FMath::Max(Dist.X, Dist.Y) * 0.41f + InCost;

	return TPair<float, float>(InCost, Predict);
}
