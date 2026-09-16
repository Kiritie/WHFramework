#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Engine/World.h"
bool UVoxelModuleStatics::IsVoxelWorldReady(const UObject*)
{
	return UVoxelModule::Get().IsReady();
}
float UVoxelModuleStatics::GetVoxelWarmupProgress(const UObject*)
{
	return UVoxelModule::Get().GetWarmupProgress();
}

FVoxelWorldSaveData UVoxelModuleStatics::GetVoxelWorldData()
{
	FVoxelWorldSaveData Data = UVoxelModule::Get().GetWorldData();
	Data.BlockSize = Data.BlockSizeCentimeters;
	return Data;
}

EVoxelWorldMode UVoxelModuleStatics::GetVoxelWorldMode()
{
	return UVoxelModule::Get().GetWorldMode();
}

void UVoxelModuleStatics::SetVoxelWorldMode(EVoxelWorldMode InWorldMode)
{
	UVoxelModule::Get().SetWorldMode(InWorldMode);
}

FVoxelTopography UVoxelModuleStatics::GetTopographyByLocation(FVector InLocation)
{
	FVoxelTopography Result;
	UVoxelModule& Module = UVoxelModule::Get();
	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();
	FIntVector Index;
	if (!Generator || !VoxelCoord::FromWorld(InLocation, Module.BlockSize(), Index))
	{
		return Result;
	}

	const FVoxelColumnSample Column = Generator->SampleColumn(Index.X, Index.Y);
	Result.Height = Column.SurfaceZ;
	Result.WaterHeight = Module.GetManifest().Settings.SeaLevel;
	switch (Column.Biome)
	{
		case EVoxelBiomeId::Forest:
			Result.BiomeType = EVoxelBiomeType::Forest;
			Result.RegionType = EVoxelRegionType::Wilderness;
			Result.Temperature = 0.65f;
			Result.Humidity = 0.75f;
			break;
		case EVoxelBiomeId::Desert:
			Result.BiomeType = EVoxelBiomeType::Desert;
			Result.RegionType = EVoxelRegionType::Desert;
			Result.Temperature = 1.f;
			Result.Humidity = 0.1f;
			break;
		case EVoxelBiomeId::Snow:
			Result.BiomeType = EVoxelBiomeType::Snow;
			Result.RegionType = EVoxelRegionType::Icefield;
			Result.Temperature = 0.f;
			Result.Humidity = 0.5f;
			break;
		case EVoxelBiomeId::Mountain:
			Result.BiomeType = EVoxelBiomeType::Mountains;
			Result.RegionType = EVoxelRegionType::Mountain;
			Result.Temperature = 0.25f;
			Result.Humidity = 0.35f;
			break;
		case EVoxelBiomeId::Ocean:
			Result.BiomeType = EVoxelBiomeType::Ocean;
			Result.RegionType = EVoxelRegionType::Ocean;
			Result.Temperature = 0.5f;
			Result.Humidity = 1.f;
			break;
		case EVoxelBiomeId::Plains:
		default:
			Result.BiomeType = EVoxelBiomeType::Plains;
			Result.RegionType = EVoxelRegionType::Plain;
			Result.Temperature = 0.6f;
			Result.Humidity = 0.45f;
			break;
	}
	return Result;
}

float UVoxelModuleStatics::GetWorldGeneratePercent()
{
	return UVoxelModule::Get().GetWarmupProgress();
}
bool UVoxelModuleStatics::CreateVoxelWorld(const UObject*, FVoxelGenerationSettings S, int32 Size, FString& E)
{
	return UVoxelModule::Get().CreateWorld(S, Size, E);
}
bool UVoxelModuleStatics::GetVoxelBlock(const UObject*, FIntVector P, FVoxelItem& O)
{
	UVoxelModule& M = UVoxelModule::Get();
	FVoxelBlockState B;
	if (!M.IsReady() || !M.GetRuntime()->TryGetBlock(P, B))
		return false;
	if (B.IsAir())
	{
		O = FVoxelItem();
		return true;
	}
	return FVoxelItemBridge::ToItem(*M.GetRegistry(), B, 1, O);
}
bool UVoxelModuleStatics::TraceVoxel(const UObject*, FVector S, FVector D, float Range, FVoxelHitResult& O)
{
	O = {};
	UVoxelModule& M = UVoxelModule::Get();
	if (!M.IsReady())
		return false;
	auto H = M.Trace(S, D, Range);
	O.bNeedsData = H.Status == EVoxelTraceStatus::NeedsData;
	if (H.Status != EVoxelTraceStatus::Hit)
		return false;
	O.bHit = true;
	O.Index = H.Index;
	O.PlacementIndex = H.PlacementIndex;
	O.Point = H.Point;
	O.Normal = H.Normal;
	return FVoxelItemBridge::ToItem(*M.GetRegistry(), H.State, 1, O.VoxelItem);
}
bool UVoxelModuleStatics::ImportVoxelPrefab(const UObject*, const FVoxelPrefabSaveData& P, FIntVector O, FString& E)
{
	return UVoxelModule::Get().ApplyPrefab(P, O, E);
}
bool UVoxelModuleStatics::ExportVoxelPrefab(const UObject*, FIntVector A, FIntVector B, FVoxelPrefabSaveData& O, FString& E)
{
	return UVoxelModule::Get().ExportPrefab(A, B, O, E);
}
bool UVoxelModuleStatics::AreCollisionsReady(UVoxelModule& M, const FBox& B)
{
	if (!M.IsReady() || !B.IsValid || B.Min.ContainsNaN() || B.Max.ContainsNaN())
		return false;
	FIntVector Lo;
	FIntVector Hi;
	if (!VoxelCoord::FromWorld(B.Min, M.BlockSize(), Lo) || !VoxelCoord::FromWorld(B.Max, M.BlockSize(), Hi))
		return false;
	auto A = VoxelCoord::Section(Lo), Z = VoxelCoord::Section(Hi);
	int64 Count = int64(Z.X - A.X + 1) * (Z.Y - A.Y + 1) * (Z.Z - A.Z + 1);
	if (Count <= 0 || Count > 512)
		return false;
	const auto& Config = M.GetManifest().Settings;
	for (int32 K = A.Z; K <= Z.Z; ++K)
		for (int32 J = A.Y; J <= Z.Y; ++J)
			for (int32 I = A.X; I <= Z.X; ++I)
			{
				if (int64(K) * 16 >= Config.MaxZ || int64(K) * 16 + 16 <= Config.MinZ)
					continue;
				const auto* S = M.GetRuntime()->Find({I, J, K});
				if (!S || S->Status != EVoxelSectionStatus::DataReady || !S->bHasCollision || S->bCollisionDirty)
					return false;
			}
	return true;
}
bool UVoxelModuleStatics::FindStandLocation(const UObject*, FVector Desired, float R, float H, float Search, FVector& O)
{
	UVoxelModule& M = UVoxelModule::Get();
	if (!FMath::IsFinite(R) || !FMath::IsFinite(H) || !FMath::IsFinite(Search) || R <= 0 || H < R || Search < 0 || Search > 4096 || Desired.ContainsNaN())
		return false;
	FVector Start = Desired + FVector(0, 0, Search), End = Desired - FVector(0, 0, Search);
	FVector Ext(R, R, H);
	if (!AreCollisionsReady(M, FBox(End - Ext, Start + Ext)))
		return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VoxelStand), false);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FHitResult Hit;
	if (!M.GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeCapsule(R, H), Q) || Hit.bStartPenetrating ||
	    Hit.ImpactNormal.Z < .65f)
		return false;
	FVector Candidate = Hit.Location + FVector(0, 0, 2);
	if (M.GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R * .99f, H * .99f), Q))
		return false;
	O = Candidate;
	return true;
}
