#include "Voxel/Generation/VoxelGenerationQuery.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/Biome/VoxelBiomeGenerator.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Ecology/VoxelNaturalDecoration.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyPlanner.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	constexpr int32 VoxelGenerationPlanTileSide = 256;

	void EnumerateQueryRequiredSources(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
	{
		static_cast<const FVoxelGenerationQuery*>(InContext)->VisitRequiredSources(InVisit);
	}

	FVoxelGenerationRequiredCapacity MeasureQueryOwnedSources(const FVoxelGenerationQuery& InQuery)
	{
		FVoxelGenerationRequiredCapacity Capacity;
		FString Error;
		if (!FVoxelGenerationRequiredSourceScope::Measure(&InQuery,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				static_cast<const FVoxelGenerationQuery*>(InContext)->VisitRequiredSources(InVisit, false);
			}, Capacity, Error))
		{
			Capacity.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)] = MAX_uint64;
			Capacity.LeaseCount = MAX_int32;
		}
		return Capacity;
	}

	bool MakeTileBounds(const FVoxelGenerationTileKey& InKey, FVoxelGenerationBounds& OutBounds)
	{
		const FInt64Vector Min(int64(InKey.Coordinate.X) * VoxelGenerationPlanTileSide,
			int64(InKey.Coordinate.Y) * VoxelGenerationPlanTileSide, int64(InKey.Coordinate.Z) * VoxelGenerationPlanTileSide);
		return VoxelGeneration::ClipWorldBounds(Min, Min + FInt64Vector(VoxelGenerationPlanTileSide), OutBounds);
	}

	template<typename GeneratorType, typename... ArgumentTypes>
	TSharedPtr<const GeneratorType, ESPMode::ThreadSafe> MakeQueryGenerator(
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease, ArgumentTypes&&... InArguments)
	{
		if (InLease)
		{
			return VoxelBoundedAllocation::MakeWorkerShared<const GeneratorType>(MoveTemp(InLease),
				Forward<ArgumentTypes>(InArguments)...);
		}
#if WITH_DEV_AUTOMATION_TESTS
		return MakeShared<const GeneratorType, ESPMode::ThreadSafe>(Forward<ArgumentTypes>(InArguments)...);
#else
		checkNoEntry();
		return nullptr;
#endif
	}
}

FVoxelGenerationQuery::FPreparedState::FPreparedState()
{
}

FVoxelGenerationQuery::FPreparedState::~FPreparedState()
{
}

FVoxelGenerationQuery::FPreparedState::FPreparedState(FPreparedState&& InOther)
{
	*this = MoveTemp(InOther);
}

FVoxelGenerationQuery::FPreparedState& FVoxelGenerationQuery::FPreparedState::operator=(FPreparedState&& InOther)
{
	if (this != &InOther)
	{
		Reset();
		Lease = MoveTemp(InOther.Lease);
		HydrologyRetention = MoveTemp(InOther.HydrologyRetention);
		Caves = MoveTemp(InOther.Caves);
		Ecology = MoveTemp(InOther.Ecology);
		Features = MoveTemp(InOther.Features);
		Structures = MoveTemp(InOther.Structures);
		Bounds = InOther.Bounds;
		Cancel = InOther.Cancel;
		bColumnsPrepared = InOther.bColumnsPrepared;
		bSymbolsPrepared = InOther.bSymbolsPrepared;
		InOther.Cancel = nullptr;
		InOther.bColumnsPrepared = false;
		InOther.bSymbolsPrepared = false;
	}
	return *this;
}

void FVoxelGenerationQuery::FPreparedState::Reset()
{
	Caves.Empty();
	Ecology.Empty();
	Features.Empty();
	Structures.Empty();
	HydrologyRetention.Reset();
	Lease.Reset();
	Cancel = nullptr;
	bColumnsPrepared = false;
	bSymbolsPrepared = false;
}

bool FVoxelGenerationQuery::FPreparedState::Reserve(
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	const int32 InTileCount,
	const int32 InEcologyCount,
	const bool bInStructuresOnly,
	const uint64 InExistingInputBytes,
	const int32 InRequiredLeaseCount,
	FString& OutError)
{
	check(!Lease && Caves.Max() == 0 && Ecology.Max() == 0 && Features.Max() == 0 && Structures.Max() == 0);
	const uint64 CaveBytes = !bInStructuresOnly && InTileCount > 0
		? VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCavePlanPtr, FDefaultAllocator>(InTileCount) : 0;
	const uint64 EcologyBytes = !bInStructuresOnly && InEcologyCount > 0
		? VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelEcologyPlanPtr, FDefaultAllocator>(InEcologyCount) : 0;
	const uint64 FeatureBytes = !bInStructuresOnly && InTileCount > 0
		? VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelFeaturePlanPtr, FDefaultAllocator>(InTileCount) : 0;
	const uint64 StructureBytes = InTileCount > 0
		? VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelStructurePlanPtr, FDefaultAllocator>(InTileCount) : 0;
	const uint64 NewBytes = CaveBytes + EcologyBytes + FeatureBytes + StructureBytes;
	if (InResources && NewBytes > 0)
	{
		FVoxelGenerationRequiredCapacity Sources;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Sources, OutError)) return false;
		const uint64 RequiredInput = FMath::Max(InExistingInputBytes,
			Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)]);
		const int32 RequiredCount = FMath::Max(InRequiredLeaseCount, Sources.LeaseCount);
		const uint64 InputLimit = InResources->Snapshot().Limits.InputBytes;
		if (RequiredInput > InputLimit || NewBytes > InputLimit - RequiredInput)
		{
			OutError = TEXT("Oversized: generation query prepared references");
			return false;
		}
		const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input, NewBytes, Lease,
			0, RequiredInput, RequiredCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
				? TEXT("CapacityUnachievable: generation query prepared references and required native input")
				: TEXT("CapacityBlocked: generation query prepared references");
			return false;
		}
	}
#if !WITH_DEV_AUTOMATION_TESTS
	else if (!InResources)
	{
		OutError = TEXT("InvalidConsumer: generation query prepared references have no original ledger");
		return false;
	}
#endif
	// 候选一次授予完整新 backing；已提交状态仍持有原 backing，覆盖替换峰值。
	if (!bInStructuresOnly)
	{
		Caves.Reserve(InTileCount);
		Ecology.Reserve(InEcologyCount);
		Features.Reserve(InTileCount);
	}
	Structures.Reserve(InTileCount);
	if (Lease)
	{
		const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Caves) +
			VoxelGenerationAllocation::AllocatedBytes(Ecology) + VoxelGenerationAllocation::AllocatedBytes(Features) +
			VoxelGenerationAllocation::AllocatedBytes(Structures);
		check(ActualBytes <= NewBytes);
		if (!Lease->Resize(ActualBytes))
		{
			OutError = TEXT("InvalidResource: generation query prepared reservation");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

FVoxelGenerationQuery::FVoxelGenerationQuery()
{
}

FVoxelGenerationQuery::~FVoxelGenerationQuery()
{
}

uint64 FVoxelGenerationQuery::GetRequiredInputBytes() const
{
	return MeasureQueryOwnedSources(*this).Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
}

int32 FVoxelGenerationQuery::GetRequiredInputLeaseCount() const
{
	return MeasureQueryOwnedSources(*this).LeaseCount;
}

void FVoxelGenerationQuery::VisitRequiredSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit,
	const bool bInIncludeCallerOutput) const
{
	if (!Resources) return;
	if (Cache)
	{
		uint64 Bytes;
		int32 Count;
		Cache->GetRequiredInputCapacity(Bytes, Count);
		if (Count) InVisit({Cache.Get(), 0, EVoxelTerrainResourcePool::Input, Bytes, Count});
	}
	if (Config)
	{
		FVoxelGenerationRequiredSourceScope::VisitLease(Config->InputLease.Get(), InVisit);
		FVoxelGenerationRequiredSourceScope::VisitLease(Config->RecipeInputLease.Get(), InVisit);
	}
	const auto VisitGenerator = [&InVisit](const auto& InGenerator)
	{
		if (InGenerator)
		{
			using FGenerator = std::remove_const_t<std::remove_reference_t<decltype(*InGenerator)>>;
			InVisit({InGenerator.Get(), 0, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<const FGenerator>()});
		}
	};
	VisitGenerator(Climate);
	VisitGenerator(Terrain);
	VisitGenerator(Biome);
	VisitGenerator(Hydrology);
	VisitGenerator(Cave);
	VisitGenerator(Aquifer);
	VisitGenerator(Surface);
	VisitGenerator(Ecology);
	const auto VisitPrepared = [this, &InVisit](const FPreparedState& InState)
	{
		FVoxelGenerationRequiredSourceScope::VisitLease(InState.Lease.Get(), InVisit);
		if (InState.HydrologyRetention)
			FVoxelGenerationRequiredSourceScope::VisitLease(&InState.HydrologyRetention->GetResourceLease().Get(), InVisit);
		const auto VisitPlans = [this, &InVisit](const auto& InPlans)
		{
			for (const auto& Plan : InPlans)
			{
				if (Plan && Plan->ResourceLease && Plan->ResourceLease->IsOwnedBy(*Resources))
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(Plan->ResourceLease.Get(), InVisit);
				}
			}
		};
		VisitPlans(InState.Caves);
		VisitPlans(InState.Ecology);
		VisitPlans(InState.Features);
		VisitPlans(InState.Structures);
	};
	VisitPrepared(Prepared);
	if (Preparing) VisitPrepared(*Preparing);
	if (bInIncludeCallerOutput && RequiredOutputLeaseCount) InVisit(RequiredOutputSource);
}

FVoxelGenerationQuery::FVoxelGenerationQuery(FVoxelGenerationQuery&& InOther)
{
	*this = MoveTemp(InOther);
}

FVoxelGenerationQuery& FVoxelGenerationQuery::operator=(FVoxelGenerationQuery&& InOther)
{
	if (this != &InOther)
	{
		check(!Preparing && !InOther.Preparing);
		Prepared = MoveTemp(InOther.Prepared);
		Config = MoveTemp(InOther.Config);
		Cache = MoveTemp(InOther.Cache);
		Resources = MoveTemp(InOther.Resources);
		Consumer = InOther.Consumer;
		GeneratorInputBytes = InOther.GeneratorInputBytes;
		InOther.GeneratorInputBytes = 0;
		RequiredOutputInputBytes = InOther.RequiredOutputInputBytes;
		RequiredOutputLeaseCount = InOther.RequiredOutputLeaseCount;
		RequiredOutputSource = InOther.RequiredOutputSource;
		InOther.RequiredOutputInputBytes = 0;
		InOther.RequiredOutputLeaseCount = 0;
		Climate = MoveTemp(InOther.Climate);
		Terrain = MoveTemp(InOther.Terrain);
		Biome = MoveTemp(InOther.Biome);
		Hydrology = MoveTemp(InOther.Hydrology);
		Cave = MoveTemp(InOther.Cave);
		Aquifer = MoveTemp(InOther.Aquifer);
		Surface = MoveTemp(InOther.Surface);
		Ecology = MoveTemp(InOther.Ecology);
		bUseColumnCache = InOther.bUseColumnCache;
		bRequireReadyHydrology = InOther.bRequireReadyHydrology;
		bRequireReadyPlans = InOther.bRequireReadyPlans;
		FCString::Strncpy(PlanSampleError, InOther.PlanSampleError, UE_ARRAY_COUNT(PlanSampleError));
		MissingHydrologyContext = MoveTemp(InOther.MissingHydrologyContext);
		MissingHydrologyObserver = InOther.MissingHydrologyObserver;
		InOther.MissingHydrologyObserver = nullptr;
	}
	return *this;
}

bool FVoxelGenerationQuery::ValidateConsumer(FString& OutError) const
{
	if (!Config || !Cache)
	{
		OutError = TEXT("Voxel query is not initialized");
		return false;
	}
	if (!Resources)
	{
#if WITH_DEV_AUTOMATION_TESTS
		return true;
#else
		OutError = TEXT("InvalidConsumer: generation query has no original World ledger");
		return false;
#endif
	}
	if (Cache->GetResources().Get() != Resources.Get() || !Consumer.IsValid() ||
		Consumer.WorldEpoch != Cache->GetControlConsumer().WorldEpoch)
	{
		OutError = TEXT("InvalidConsumer: generation query lost its original World identity");
		return false;
	}
	FVoxelGenerationConsumerKey CurrentConsumer;
	if (!Cache->ResolveConsumer(CurrentConsumer, OutError))
	{
		return false;
	}
	if (const auto* ScopeConsumer = FVoxelGenerationConsumerScope::GetConsumerKey();
		ScopeConsumer && (!ScopeConsumer->IsValid() || !(*ScopeConsumer == Consumer)))
	{
		OutError = TEXT("InvalidConsumer: generation query belongs to another execution consumer");
		return false;
	}
	if (const auto* CapturedConsumer = FVoxelGenerationPlanningReadScope::GetConsumerKey();
		CapturedConsumer && !(*CapturedConsumer == Consumer))
	{
		OutError = TEXT("InvalidConsumer: generation query belongs to another logical consumer");
		return false;
	}
	return true;
}

bool FVoxelGenerationQuery::RetainHydrologyReads(
	const FVoxelGenerationBounds& InBounds,
	const int64 InMargin,
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutRetention,
	FString& OutError) const
{
	// Ready-only 任务沿原预检闭包保留；冷构建必须让依赖活过本次自然采样。
	if (!Resources || bRequireReadyHydrology || bRequireReadyPlans) return true;
	const auto& Settings = Config->Recipe->Settings;
	const int64 Margin = FMath::Max<int64>(0, InMargin);
	const auto At = [&Settings](const int64 X, const int64 Y)
	{
		return HydrologyKeyForVoxel(
			int32(FMath::Clamp<int64>(X, VoxelGeneration::WorldCellMin, VoxelGeneration::WorldCellMaxExclusive - 1)),
			int32(FMath::Clamp<int64>(Y, VoxelGeneration::WorldCellMin, VoxelGeneration::WorldCellMaxExclusive - 1)), Settings).Coordinate;
	};
	const FIntPoint First = At(int64(InBounds.Min.X) - Margin, int64(InBounds.Min.Y) - Margin);
	const FIntPoint Last = At(int64(InBounds.Max.X) - 1 + Margin, int64(InBounds.Max.Y) - 1 + Margin);
	const int64 Side = int64(Settings.HydrologyRegionSide) * Settings.HydrologyCellSize;
	const FInt64Vector2 FirstCenter(int64(First.X) * Side + Side / 2, int64(First.Y) * Side + Side / 2);
	const FInt64Vector2 LastCenter(int64(Last.X) * Side + Side / 2, int64(Last.Y) * Side + Side / 2);
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = FIntPoint(int32(FMath::Clamp<int64>((FirstCenter.X + LastCenter.X) / 2, MIN_int32, MAX_int32)),
		int32(FMath::Clamp<int64>((FirstCenter.Y + LastCenter.Y) / 2, MIN_int32, MAX_int32)));
	const int64 Radius = FMath::Max(
		FMath::Max(FMath::Abs(FirstCenter.X - Point.Center.X), FMath::Abs(LastCenter.X - Point.Center.X)),
		FMath::Max(FMath::Abs(FirstCenter.Y - Point.Center.Y), FMath::Abs(LastCenter.Y - Point.Center.Y)));
	if (Radius > MAX_int32)
	{
		OutError = TEXT("Oversized: query hydrology retention range");
		return false;
	}
	Point.HydrologyRadiusCells = int32(Radius);
	Point.NaturalRadiusCells = -1;
	Point.PlanRadiusCells = -1;
	return Cache->RetainForTask(Point, Resources.ToSharedRef(), Consumer, OutRetention, OutError);
}

bool FVoxelGenerationQuery::GetPlanKeyRanges(const FVoxelGenerationBounds& InBounds,
	const FVoxelGenerationSettings& InSettings, FVoxelGenerationPlanKeyRanges& OutRanges, FString& OutError)
{
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Invalid generation planning bounds");
		return false;
	}
	const auto At = [](const int64 Value, const int32 Side)
	{
		return VoxelGeneration::FloorDivide(int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32 - 1)), Side);
	};
	const int64 Margin = FMath::Max(InSettings.CaveSpacing, GenerationPlanTileSide);
	const int64 EcologyMargin = int64(FMath::Max(0, FMath::Max(
		InSettings.Ecology.Tree.CrownRadius, InSettings.Ecology.Grass.PatchRadius))) + 2;
	FVoxelGenerationPlanKeyRanges Candidate;
	Candidate.TileMin = FIntPoint(At(int64(InBounds.Min.X) - Margin, GenerationPlanTileSide),
		At(int64(InBounds.Min.Y) - Margin, GenerationPlanTileSide));
	Candidate.TileMax = FIntPoint(At(int64(InBounds.Max.X) - 1 + Margin, GenerationPlanTileSide),
		At(int64(InBounds.Max.Y) - 1 + Margin, GenerationPlanTileSide));
	Candidate.EcologyMin = FIntPoint(At(int64(InBounds.Min.X) - EcologyMargin, EcologyTileSide),
		At(int64(InBounds.Min.Y) - EcologyMargin, EcologyTileSide));
	Candidate.EcologyMax = FIntPoint(At(int64(InBounds.Max.X) - 1 + EcologyMargin, EcologyTileSide),
		At(int64(InBounds.Max.Y) - 1 + EcologyMargin, EcologyTileSide));
	const int64 Tiles = (int64(Candidate.TileMax.X) - Candidate.TileMin.X + 1) *
		(int64(Candidate.TileMax.Y) - Candidate.TileMin.Y + 1);
	const int64 EcologyCount = (int64(Candidate.EcologyMax.X) - Candidate.EcologyMin.X + 1) *
		(int64(Candidate.EcologyMax.Y) - Candidate.EcologyMin.Y + 1);
	if (Tiles <= 0 || EcologyCount <= 0 || Tiles > MAX_int32 || EcologyCount > MAX_int32)
	{
		OutError = TEXT("Oversized: generation planning key ranges");
		return false;
	}
	Candidate.TileCount = int32(Tiles);
	Candidate.EcologyCount = int32(EcologyCount);
	OutRanges = Candidate;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::GetStructureKeyRange(
	const FVoxelGenerationBounds& InBounds,
	FVoxelGenerationStructureKeyRange& OutRange,
	FString& OutError)
{
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Invalid structure planning bounds");
		return false;
	}
	const auto At = [](const int64 InValue)
	{
		return int32(VoxelGeneration::FloorDivideWide(
			FMath::Clamp<int64>(InValue, VoxelGeneration::WorldCellMin, VoxelGeneration::WorldCellMaxExclusive - 1),
			GenerationPlanTileSide));
	};
	FVoxelGenerationStructureKeyRange Candidate;
	Candidate.Min = FIntPoint(At(int64(InBounds.Min.X) - GenerationPlanTileSide),
		At(int64(InBounds.Min.Y) - GenerationPlanTileSide));
	Candidate.Max = FIntPoint(At(int64(InBounds.Max.X) - 1 + GenerationPlanTileSide),
		At(int64(InBounds.Max.Y) - 1 + GenerationPlanTileSide));
	const int64 Count = (int64(Candidate.Max.X) - Candidate.Min.X + 1) *
		(int64(Candidate.Max.Y) - Candidate.Min.Y + 1);
	if (Count <= 0 || Count > MAX_int32)
	{
		OutError = TEXT("Oversized: structure planning key range");
		return false;
	}
	Candidate.Count = int32(Count);
	OutRange = Candidate;
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
FVoxelGenerationPlanKeys FVoxelGenerationQuery::GatherPlanKeys(
	const FVoxelGenerationBounds& InBounds,
	const FVoxelGenerationSettings& InSettings)
{
	FVoxelGenerationPlanKeys Keys;
	FVoxelGenerationPlanKeyRanges Ranges;
	FString Error;
	if (!GetPlanKeyRanges(InBounds, InSettings, Ranges, Error)) return Keys;
	Keys.Tiles.Reserve(Ranges.TileCount);
	Keys.EcologyTiles.Reserve(Ranges.EcologyCount);
	for (int64 Y = Ranges.TileMin.Y; Y <= Ranges.TileMax.Y; ++Y)
	{
		for (int64 X = Ranges.TileMin.X; X <= Ranges.TileMax.X; ++X)
		{
			Keys.Tiles.Add({ FIntVector(int32(X), int32(Y), 0) });
		}
	}
	for (int64 Y = Ranges.EcologyMin.Y; Y <= Ranges.EcologyMax.Y; ++Y)
	{
		for (int64 X = Ranges.EcologyMin.X; X <= Ranges.EcologyMax.X; ++X)
		{
			Keys.EcologyTiles.Add({ FIntPoint(int32(X), int32(Y)) });
		}
	}
	return Keys;
}

void FVoxelGenerationQuery::GatherStructureTiles(
	const FVoxelGenerationBounds& InBounds,
	TArray<FVoxelGenerationTileKey>& OutTiles)
{
	FVoxelGenerationStructureKeyRange Range;
	FString Error;
	if (!GetStructureKeyRange(InBounds, Range, Error))
	{
		return;
	}
	TArray<FVoxelGenerationTileKey> Tiles;
	Tiles.Reserve(Range.Count);
	for (int64 Y = Range.Min.Y; Y <= Range.Max.Y; ++Y)
	{
		for (int64 X = Range.Min.X; X <= Range.Max.X; ++X)
		{
			Tiles.Add({ FIntVector(int32(X), int32(Y), 0) });
		}
	}
	OutTiles = MoveTemp(Tiles);
}
#endif

FVoxelHydrologyRegionKey FVoxelGenerationQuery::HydrologyKeyForVoxel(
	const int32 InX,
	const int32 InY,
	const FVoxelGenerationSettings& InSettings)
{
	const int32 CellSize = FMath::Max(1, InSettings.HydrologyCellSize);
	const int32 RegionSide = FMath::Max(1, InSettings.HydrologyRegionSide);
	return { FIntPoint(
		VoxelGeneration::FloorDivide(VoxelGeneration::FloorDivide(InX, CellSize), RegionSide),
		VoxelGeneration::FloorDivide(VoxelGeneration::FloorDivide(InY, CellSize), RegionSide)) };
}

bool FVoxelGenerationQuery::SamplePlanColumn(
	const FIntVector& InPosition,
	FVoxelColumnSample& OutColumn,
	const TAtomic<bool>* InCancel) const
{
	FString Error;
	if (SampleEnvironmentColumn(InPosition.X, InPosition.Y,
		OutColumn, Error, InCancel)) return true;
	FCString::Strncpy(PlanSampleError, Error.IsEmpty() ? TEXT("Voxel plan column sampling failed") : *Error,
		UE_ARRAY_COUNT(PlanSampleError));
	return false;
}

bool FVoxelGenerationQuery::SamplePlanSymbol(
	const FIntVector& InPosition,
	uint32& OutSymbol,
	const TAtomic<bool>* InCancel) const
{
	FVoxelColumnSample Column;
	if (!SamplePlanColumn(InPosition, Column, InCancel)) return false;
	const int32 Density = Terrain->SampleDensityQ16(InPosition,
		{ Column.SurfaceZ, Column.DensityHeight, Column.SlopePermille });
	OutSymbol = Density > 0 ? Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
	return true;
}

bool FVoxelGenerationQuery::EnsureHydrologyPlan(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_QueryHydrologyPlan);
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	const auto& Settings = Config->Recipe->Settings;
	const int64 MinCell = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, Settings.HydrologyCellSize);
	const int64 MaxCell = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, Settings.HydrologyCellSize) + 1;
	const FInt64Vector2 Core(int64(InKey.Coordinate.X) * Settings.HydrologyRegionSide,
		int64(InKey.Coordinate.Y) * Settings.HydrologyRegionSide);
	if (Core.X >= MaxCell || Core.Y >= MaxCell || Core.X + Settings.HydrologyRegionSide <= MinCell ||
		Core.Y + Settings.HydrologyRegionSide <= MinCell)
	{
		OutError = TEXT("Invalid hydrology region outside world cell domain");
		return false;
	}
	if (bRequireReadyPlans && Cache->FindHydrology(InKey, OutPlan))
	{
		OutError.Reset();
		return true;
	}
	if (bRequireReadyPlans)
	{
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Hydrology,
			FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0));
		OutError = TEXT("DependencyNotReady: Hydrology");
		return false;
	}
	return Cache->GetOrBuildHydrology(InKey,
		[this, InKey, InCancel](FVoxelHydrologyPlan& Plan, FString& Error)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_HydrologyPlanBuild);
			return Hydrology->BuildPlan(InKey, Plan, Error, InCancel);
		}, OutPlan, OutError, InCancel);
}

bool FVoxelGenerationQuery::EnsureCavePlan(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	FVoxelGenerationBounds Bounds;
	if (!MakeTileBounds(InKey, Bounds)) { OutError = TEXT("Invalid cave tile outside world cell domain"); return false; }
	if (bRequireReadyPlans)
	{
		if (Cache->FindCave(InKey, OutPlan))
		{
			OutError.Reset();
			return true;
		}
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Cave, InKey.Coordinate);
		OutError = TEXT("DependencyNotReady: Cave");
		return false;
	}
	return Cache->GetOrBuildCave(InKey,
		[this, Bounds, InCancel](FVoxelCavePlan& Plan, FString& Error)
		{
			TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
			if (!Preparing && !RetainHydrologyReads(Bounds, FVoxelCaveGenerator::GetMaximumReach(Config->Recipe->Settings), Retention, Error)) return false;
			FVoxelGenerationRequiredSourceScope RetentionSource(Resources.Get(), Retention ? &Retention->GetResourceLease().Get() : nullptr);
			PlanSampleError[0] = 0;
			auto ColumnSampler = [this, InCancel](
				const FIntVector& Position, FVoxelColumnSample& Column)
			{
				return SamplePlanColumn(Position, Column, InCancel);
			};
			const bool bBuilt = Cave->BuildPlan(Bounds, ColumnSampler, Plan, Error, InCancel);
			if (PlanSampleError[0] != 0) Error = PlanSampleError;
			return bBuilt && (PlanSampleError[0] == 0);
		}, OutPlan, OutError, InCancel);
}

bool FVoxelGenerationQuery::EnsureStructurePlan(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	FVoxelGenerationBounds Bounds;
	if (!MakeTileBounds(InKey, Bounds)) { OutError = TEXT("Invalid structure tile outside world cell domain"); return false; }
	if (bRequireReadyPlans)
	{
		if (Cache->FindStructure(InKey, OutPlan))
		{
			OutError.Reset();
			return true;
		}
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Structure, InKey.Coordinate);
		OutError = TEXT("DependencyNotReady: Structure");
		return false;
	}
	return Cache->GetOrBuildStructure(InKey,
		[this, Bounds, InCancel](FVoxelStructurePlan& Plan, FString& Error)
		{
			TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
			if (!Preparing && !RetainHydrologyReads(Bounds, 0, Retention, Error)) return false;
			FVoxelGenerationRequiredSourceScope RetentionSource(Resources.Get(), Retention ? &Retention->GetResourceLease().Get() : nullptr);
			PlanSampleError[0] = 0;
			auto ColumnSampler = [this, InCancel](
				const FIntVector& Position, FVoxelColumnSample& Column)
			{
				return SamplePlanColumn(Position, Column, InCancel);
			};
			FVoxelStructurePlanner Planner(Config->Recipe.ToSharedRef());
			const bool bBuilt = Planner.BuildPlan(Bounds, ColumnSampler, Plan, Error, InCancel);
			if (PlanSampleError[0] != 0) Error = PlanSampleError;
			return bBuilt && (PlanSampleError[0] == 0);
		}, OutPlan, OutError, InCancel);
}

bool FVoxelGenerationQuery::EnsureFeaturePlan(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	FVoxelGenerationBounds Bounds;
	if (!MakeTileBounds(InKey, Bounds)) { OutError = TEXT("Invalid feature tile outside world cell domain"); return false; }
	if (bRequireReadyPlans)
	{
		if (Cache->FindFeature(InKey, OutPlan))
		{
			OutError.Reset();
			return true;
		}
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Feature, InKey.Coordinate);
		OutError = TEXT("DependencyNotReady: Feature");
		return false;
	}
	return Cache->GetOrBuildFeature(InKey,
		[this, Bounds, InCancel](FVoxelFeaturePlan& Plan, FString& Error)
		{
			int64 Margin = 0;
			for (const auto& Feature : Config->Recipe->Features) Margin = FMath::Max<int64>(Margin, Feature.Placement.Spacing - 1);
			TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
			if (!Preparing && !RetainHydrologyReads(Bounds, Margin, Retention, Error)) return false;
			FVoxelGenerationRequiredSourceScope RetentionSource(Resources.Get(), Retention ? &Retention->GetResourceLease().Get() : nullptr);
			PlanSampleError[0] = 0;
			FVoxelGenerationWorkspace SamplesWorkspace;
			TMap<FIntPoint, FVoxelColumnSample> Samples;
			int32 SampleCapacity = 0;
			ON_SCOPE_EXIT { SamplesWorkspace.FreeMap(Samples); };
			auto ColumnSampler = [this, InCancel, &SamplesWorkspace, &Samples, &SampleCapacity](
				const FIntVector& Position, FVoxelColumnSample& Column)
			{
				FString SamplingError;
				const auto Fail = [this](const FString& InError)
				{
					FCString::Strncpy(PlanSampleError, *InError, UE_ARRAY_COUNT(PlanSampleError));
					return false;
				};
				if (!ValidateConsumer(SamplingError)) return Fail(SamplingError);
				if (!FVoxelGenerationPlanningReadScope::CanPublish())
					return Fail(FVoxelGenerationPlanningReadScope::PublishBlockError());
				const FIntPoint XY(Position.X, Position.Y);
				if (const auto* Existing = Samples.Find(XY)) { Column = *Existing; return true; }
				if (!SamplePlanColumn(Position, Column, InCancel)) return false;
				if (!FVoxelGenerationPlanningReadScope::CanPublish())
					return Fail(FVoxelGenerationPlanningReadScope::PublishBlockError());
				if (Samples.Num() == SampleCapacity)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FeatureColumnCacheGrowth);
					if (SampleCapacity == MAX_int32) return Fail(TEXT("Oversized: feature sample cache capacity"));
					const int32 NextCapacity = int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(64, int64(SampleCapacity) * 2)));
					if (!SamplesWorkspace.ReserveMap(Samples, NextCapacity, SamplingError)) return Fail(SamplingError);
					SampleCapacity = NextCapacity;
				}
				Samples.Add(XY, Column);
				return true;
			};
			auto SymbolSampler = [this, &ColumnSampler](
				const FIntVector& Position, uint32& Symbol)
			{
				// 自然列只取决于 XY；每个矿脉格仍用原坐标计算自身密度，不复用方块结果。
				FVoxelColumnSample Column;
				if (!ColumnSampler(Position, Column)) return false;
				const int32 Density = Terrain->SampleDensityQ16(Position,
					{Column.SurfaceZ, Column.DensityHeight, Column.SlopePermille});
				Symbol = Density > 0 ? Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
				return true;
			};
			FVoxelFeaturePlanner Planner(Config->Recipe.ToSharedRef());
			const bool bBuilt = Planner.BuildPlan(Bounds, ColumnSampler, SymbolSampler,
				Plan, Error, InCancel);
			if (PlanSampleError[0] != 0) Error = PlanSampleError;
			return bBuilt && (PlanSampleError[0] == 0);
		}, OutPlan, OutError, InCancel);
}

bool FVoxelGenerationQuery::EnsureEcologyPlan(
	const FVoxelEcologyTileKey& InKey,
	FVoxelEcologyPlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	const FInt64Vector Min(int64(InKey.Coordinate.X) * EcologyTileSide, int64(InKey.Coordinate.Y) * EcologyTileSide,
		Config->Recipe->Settings.MinZ);
	FVoxelGenerationBounds Bounds;
	if (!VoxelGeneration::ClipWorldBounds(Min, FInt64Vector(Min.X + EcologyTileSide, Min.Y + EcologyTileSide,
		Config->Recipe->Settings.MaxZ), Bounds)) { OutError = TEXT("Invalid ecology tile outside world cell domain"); return false; }
	if (bRequireReadyPlans)
	{
		if (Cache->FindEcology(InKey, OutPlan))
		{
			OutError.Reset();
			return true;
		}
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Ecology,
			FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0));
		OutError = TEXT("DependencyNotReady: Ecology");
		return false;
	}
	return Cache->GetOrBuildEcology(InKey,
		[this, Bounds, InCancel](FVoxelEcologyPlan& Plan, FString& Error)
		{
			const auto& EcologySettings = Config->Recipe->Settings.Ecology;
			TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
			if (!Preparing && !RetainHydrologyReads(Bounds, FMath::Max(EcologySettings.Grass.PatchRadius, EcologySettings.Flower.PatchRadius), Retention, Error)) return false;
			FVoxelGenerationRequiredSourceScope RetentionSource(Resources.Get(), Retention ? &Retention->GetResourceLease().Get() : nullptr);
			PlanSampleError[0] = 0;
			auto ColumnSampler = [this, InCancel](
				const FIntVector& Position, FVoxelColumnSample& Column)
			{
				return SamplePlanColumn(Position, Column, InCancel);
			};
			auto SymbolSampler = [this, InCancel](
				const FIntVector& Position, uint32& Symbol)
			{
				return SamplePlanSymbol(Position, Symbol, InCancel);
			};
			const bool bBuilt = Ecology->BuildPlan(Bounds, ColumnSampler, SymbolSampler,
				Plan, Error, InCancel);
			if (PlanSampleError[0] != 0) Error = PlanSampleError;
			return bBuilt && (PlanSampleError[0] == 0);
		}, OutPlan, OutError, InCancel);
}

bool FVoxelGenerationQuery::Create(
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	FVoxelGenerationQuery& OutQuery,
	FString& OutError,
	const bool bInUseColumnCache,
	const bool bInRequireReadyHydrology,
	const bool bInRequireReadyPlans,
	const FVoxelGenerationConsumerKey* InConsumer,
	const FVoxelTerrainResourceLease* InRequiredOutputLease)
{
	if (!InConfig->IsValid() ||
		!InConfig->Recipe)
	{
		OutError =
			TEXT("Voxel generation query received an invalid runtime config");

		return false;
	}

	const auto& QueryResources = InCache->GetResources();
	FVoxelGenerationRequiredSourceScope OldSources(OutQuery.Resources.Get(), &OutQuery, EnumerateQueryRequiredSources);
	struct FCreateSources
	{
		const FVoxelGenerationRuntimeConfig& Config;
		const FVoxelGenerationPlanCache& Cache;
		const FVoxelTerrainResourceLease* Output;
	};
	const FCreateSources CreateSources{InConfig.Get(), InCache.Get(),
		QueryResources && InRequiredOutputLease && InRequiredOutputLease->IsOwnedBy(*QueryResources) ? InRequiredOutputLease : nullptr};
	FVoxelGenerationRequiredSourceScope NewSources(QueryResources.Get(), &CreateSources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Source = *static_cast<const FCreateSources*>(InContext);
			uint64 Bytes;
			int32 Count;
			Source.Cache.GetRequiredInputCapacity(Bytes, Count);
			if (Count) InVisit({&Source.Cache, 0, EVoxelTerrainResourcePool::Input, Bytes, Count});
			FVoxelGenerationRequiredSourceScope::VisitLease(Source.Config.InputLease.Get(), InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Source.Config.RecipeInputLease.Get(), InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Source.Output, InVisit);
		});
	FVoxelGenerationConsumerKey QueryConsumer;
	uint64 NewGeneratorInputBytes = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GeneratorLeases[8];
	if (QueryResources)
	{
		if ((InConfig->InputLease && (!InConfig->InputLease->IsOwnedBy(*QueryResources) ||
			InConfig->InputLease->GetPool() != EVoxelTerrainResourcePool::Input)) ||
			(InConfig->RecipeInputLease && (!InConfig->RecipeInputLease->IsOwnedBy(*QueryResources) ||
			InConfig->RecipeInputLease->GetPool() != EVoxelTerrainResourcePool::Input)))
		{
			OutError = TEXT("InvalidResource: frozen generation snapshot belongs to another original Fact ledger");
			return false;
		}
#if !WITH_DEV_AUTOMATION_TESTS
		if (!InConfig->InputLease || !InConfig->RecipeInputLease)
		{
			OutError = TEXT("InvalidResource: generation query requires a pregranted frozen snapshot");
			return false;
		}
#endif
		const auto* ScopeConsumer = FVoxelGenerationConsumerScope::GetConsumerKey();
		const auto* ReadConsumer = FVoxelGenerationPlanningReadScope::GetConsumerKey();
		if (!InConsumer && !ScopeConsumer && !ReadConsumer &&
			!FVoxelGenerationPlanningReadScope::GetTaskStamp())
		{
			OutError = TEXT("InvalidConsumer: generation query requires the original explicit or TLS consumer");
			return false;
		}
		if (!InCache->ResolveConsumer(QueryConsumer, OutError)) return false;
		const FVoxelGenerationConsumerKey* OriginalConsumer = InConsumer ? InConsumer :
			(ScopeConsumer ? ScopeConsumer : ReadConsumer);
		if (OriginalConsumer)
		{
			if (!OriginalConsumer->IsValid() || OriginalConsumer->WorldEpoch != InCache->GetControlConsumer().WorldEpoch ||
				(ScopeConsumer && (!ScopeConsumer->IsValid() || !(*OriginalConsumer == *ScopeConsumer))) ||
				(ReadConsumer && (!ReadConsumer->IsValid() || !(*OriginalConsumer == *ReadConsumer))))
			{
				OutError = TEXT("InvalidConsumer: generation query requires the original World consumer");
				return false;
			}
			QueryConsumer = *OriginalConsumer;
		}
		const uint64 GeneratorBytes[] = {
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelClimateGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelTerrainGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelBiomeGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelHydrologyPlanner>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelCaveGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelAquiferGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelSurfaceGenerator>(),
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelEcologyGenerator>()
		};
		for (const uint64 Bytes : GeneratorBytes)
		{
			NewGeneratorInputBytes += Bytes;
		}
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*QueryResources, nullptr, Required, OutError)) return false;
		const uint64 ExistingInputBytes = Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
		const int32 RequiredLeaseCount = Required.LeaseCount;
		const uint64 InputLimit = QueryResources->Snapshot().Limits.InputBytes;
		if (ExistingInputBytes > InputLimit || NewGeneratorInputBytes > InputLimit - ExistingInputBytes)
		{
			OutError = TEXT("Oversized: generation query native generator replacement");
			return false;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GeneratorsLease;
		const auto Admission = QueryResources->TryAcquire(QueryConsumer, EVoxelTerrainResourcePool::Input,
			NewGeneratorInputBytes, GeneratorsLease, 7, ExistingInputBytes, RequiredLeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
				? TEXT("CapacityUnachievable: generation query generators and required native input")
				: TEXT("CapacityBlocked: generation query native generators");
			return false;
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(GeneratorBytes) - 1; ++Index)
		{
			GeneratorLeases[Index] = GeneratorsLease->Split(GeneratorBytes[Index]);
			check(GeneratorLeases[Index]);
		}
		GeneratorLeases[7] = MoveTemp(GeneratorsLease);
	}
#if !WITH_DEV_AUTOMATION_TESTS
	else
	{
		OutError = TEXT("InvalidConsumer: generation query cache has no resource domain");
		return false;
	}
#endif

	FVoxelGenerationQuery Query;

	Query.Config = InConfig;
	Query.Cache = InCache;
	Query.Resources = QueryResources;
	Query.Consumer = QueryConsumer;
	Query.GeneratorInputBytes = NewGeneratorInputBytes;
	if (QueryResources && InRequiredOutputLease && InRequiredOutputLease->IsOwnedBy(*QueryResources))
	{
		Query.RequiredOutputInputBytes = InRequiredOutputLease->GetPool() == EVoxelTerrainResourcePool::Input ? InRequiredOutputLease->Bytes() : 0;
		Query.RequiredOutputLeaseCount = 1;
		Query.RequiredOutputSource = {nullptr, InRequiredOutputLease->Serial(),
			InRequiredOutputLease->GetPool(), InRequiredOutputLease->Bytes()};
	}
	Query.bUseColumnCache = bInUseColumnCache;
	Query.bRequireReadyHydrology = bInRequireReadyHydrology || FVoxelGenerationPlanningReadScope::RequiresReadyPlans();
	Query.bRequireReadyPlans = bInRequireReadyPlans || FVoxelGenerationPlanningReadScope::RequiresReadyPlans();

	Query.Climate = MakeQueryGenerator<FVoxelClimateGenerator>(MoveTemp(GeneratorLeases[0]), InConfig->Recipe.ToSharedRef());
	Query.Terrain = MakeQueryGenerator<FVoxelTerrainGenerator>(MoveTemp(GeneratorLeases[1]),
		InConfig->Recipe.ToSharedRef(), Query.Climate.ToSharedRef());
	Query.Biome = MakeQueryGenerator<FVoxelBiomeGenerator>(MoveTemp(GeneratorLeases[2]), InConfig->Recipe.ToSharedRef());
	Query.Hydrology = MakeQueryGenerator<FVoxelHydrologyPlanner>(MoveTemp(GeneratorLeases[3]),
		InConfig->Recipe.ToSharedRef(), Query.Terrain.ToSharedRef());
	Query.Cave = MakeQueryGenerator<FVoxelCaveGenerator>(MoveTemp(GeneratorLeases[4]), InConfig->Recipe.ToSharedRef());
	Query.Aquifer = MakeQueryGenerator<FVoxelAquiferGenerator>(MoveTemp(GeneratorLeases[5]), InConfig->Recipe.ToSharedRef());
	Query.Surface = MakeQueryGenerator<FVoxelSurfaceGenerator>(MoveTemp(GeneratorLeases[6]), InConfig->Recipe.ToSharedRef());
	Query.Ecology = MakeQueryGenerator<FVoxelEcologyGenerator>(MoveTemp(GeneratorLeases[7]), InConfig->Recipe.ToSharedRef());

	OutQuery =
		MoveTemp(Query);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareColumns(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	if (!ValidateConsumer(OutError))
	{
		return false;
	}
	if (!InBounds.IsValid())
	{
		OutError =
			TEXT("Voxel generation query bounds are invalid");

		return false;
	}

	if (InCancel &&
		InCancel->Load())
	{
		OutError =
			TEXT("Canceled");

		return false;
	}

	FPreparedState Candidate;
	Candidate.Bounds = InBounds;
	Candidate.Cancel = InCancel;
	Candidate.bColumnsPrepared = true;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	if (!RetainHydrologyReads(InBounds, 0, Candidate.HydrologyRetention, OutError)) return false;
	Prepared = MoveTemp(Candidate);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareEnvironmentColumnRead(
	const int32 InX,
	const int32 InY,
	FString& OutError)
{
	if (!ValidateConsumer(OutError)) return false;
	if (!FVoxelGenerationPlanningReadScope::CanPublish())
	{
		OutError = FVoxelGenerationPlanningReadScope::PublishBlockError();
		return false;
	}
	if (!VoxelGeneration::IsWorldCell(InX) || !VoxelGeneration::IsWorldCell(InY))
	{
		OutError = TEXT("Invalid environment read outside world cell domain");
		return false;
	}
	// 原预检 reader 已持有完整自然闭包；仅冷操作随实际读取增长原保留范围。
	if (!Resources || bRequireReadyHydrology || bRequireReadyPlans)
	{
		OutError.Reset();
		return true;
	}
	if (Prepared.bSymbolsPrepared || !Prepared.Structures.IsEmpty())
	{
		OutError = TEXT("InvalidQueryState: environment read retention cannot replace prepared symbol plans");
		return false;
	}
	const auto& Settings = Config->Recipe->Settings;
	const auto Key = HydrologyKeyForVoxel(InX, InY, Settings);
	const int64 Side = int64(Settings.HydrologyRegionSide) * Settings.HydrologyCellSize;
	const int64 X = int64(Key.Coordinate.X) * Side;
	const int64 Y = int64(Key.Coordinate.Y) * Side;
	FVoxelGenerationBounds Bounds{
		{int32(FMath::Max<int64>(VoxelGeneration::WorldCellMin, X)),
			int32(FMath::Max<int64>(VoxelGeneration::WorldCellMin, Y)), 0},
		{int32(FMath::Min<int64>(VoxelGeneration::WorldCellMaxExclusive, X + Side)),
			int32(FMath::Min<int64>(VoxelGeneration::WorldCellMaxExclusive, Y + Side)), 1}};
	if (Prepared.bColumnsPrepared && Prepared.HydrologyRetention)
	{
		if (Prepared.Bounds.Min.X <= Bounds.Min.X && Prepared.Bounds.Min.Y <= Bounds.Min.Y &&
			Prepared.Bounds.Max.X >= Bounds.Max.X && Prepared.Bounds.Max.Y >= Bounds.Max.Y)
		{
			OutError.Reset();
			return true;
		}
		Bounds.Min = Prepared.Bounds.Min.ComponentMin(Bounds.Min);
		Bounds.Max = Prepared.Bounds.Max.ComponentMax(Bounds.Max);
	}
	return PrepareColumns(Bounds, OutError);
}

bool FVoxelGenerationQuery::Prepare(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_QueryPrepare);

	if (!ValidateConsumer(OutError))
	{
		return false;
	}
	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	FVoxelGenerationPlanKeyRanges Ranges;
	if (!GetPlanKeyRanges(InBounds, Settings, Ranges, OutError)) return false;
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FPreparedState Candidate;
	TGuardValue<const FPreparedState*> PreparingGuard(Preparing, &Candidate);
	int64 NaturalMargin = FVoxelCaveGenerator::GetMaximumReach(Settings);
	for (const auto& Feature : Config->Recipe->Features) NaturalMargin = FMath::Max<int64>(NaturalMargin, Feature.Placement.Spacing - 1);
	const int64 EcologyMargin = FMath::Max(Settings.Ecology.Grass.PatchRadius, Settings.Ecology.Flower.PatchRadius);
	FVoxelGenerationBounds NaturalBounds = InBounds;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		NaturalBounds.Min[Axis] = int32(FMath::Clamp<int64>(FMath::Min(
			int64(Ranges.TileMin[Axis]) * GenerationPlanTileSide - NaturalMargin,
			int64(Ranges.EcologyMin[Axis]) * EcologyTileSide - EcologyMargin), VoxelGeneration::WorldCellMin, VoxelGeneration::WorldCellMaxExclusive - 1));
		NaturalBounds.Max[Axis] = int32(FMath::Clamp<int64>(FMath::Max(
			(int64(Ranges.TileMax[Axis]) + 1) * GenerationPlanTileSide + NaturalMargin,
			(int64(Ranges.EcologyMax[Axis]) + 1) * EcologyTileSide + EcologyMargin), VoxelGeneration::WorldCellMin + 1, VoxelGeneration::WorldCellMaxExclusive));
	}
	if (!RetainHydrologyReads(NaturalBounds, 0, Candidate.HydrologyRetention, OutError)) return false;
	const uint64 ExistingInputBytes = GetRequiredInputBytes() + RequiredOutputInputBytes;
	if (!Candidate.Reserve(Resources, Consumer, Ranges.TileCount, Ranges.EcologyCount, false, ExistingInputBytes, GetRequiredInputLeaseCount() + RequiredOutputLeaseCount, OutError))
	{
		return false;
	}
	for (int64 Y = Ranges.TileMin.Y; Y <= Ranges.TileMax.Y; ++Y)
	{
		for (int64 X = Ranges.TileMin.X; X <= Ranges.TileMax.X; ++X)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FVoxelGenerationTileKey Key{FIntVector(int32(X), int32(Y), 0)};
			FVoxelCavePlanPtr CavePlan;
			if (!EnsureCavePlan(Key, CavePlan, OutError, InCancel)) return false;
			Candidate.Caves.Add(CavePlan);
			FVoxelStructurePlanPtr StructurePlan;
			if (!EnsureStructurePlan(Key, StructurePlan, OutError, InCancel)) return false;
			Candidate.Structures.Add(StructurePlan);
			FVoxelFeaturePlanPtr FeaturePlan;
			if (!EnsureFeaturePlan(Key, FeaturePlan, OutError, InCancel)) return false;
			Candidate.Features.Add(FeaturePlan);
		}
	}

	for (int64 Y = Ranges.EcologyMin.Y; Y <= Ranges.EcologyMax.Y; ++Y)
	{
		for (int64 X = Ranges.EcologyMin.X; X <= Ranges.EcologyMax.X; ++X)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FVoxelEcologyTileKey Key{FIntPoint(int32(X), int32(Y))};
			FVoxelEcologyPlanPtr Plan;
			if (!EnsureEcologyPlan(Key, Plan, OutError, InCancel)) return false;
			Candidate.Ecology.Add(Plan);
		}
	}

	// 在查询准备阶段剔除无关计划，避免对每个方块重复遍历整圈瓦片的空查询。
	Candidate.Caves.RemoveAll([&InBounds](const FVoxelCavePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	Candidate.Ecology.RemoveAll([&InBounds](const FVoxelEcologyPlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	Candidate.Structures.RemoveAll([&InBounds](const FVoxelStructurePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	Candidate.Features.RemoveAll([&InBounds](const FVoxelFeaturePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	Candidate.Bounds = InBounds;
	Candidate.Cancel = InCancel;
	Candidate.bColumnsPrepared = true;
	Candidate.bSymbolsPrepared = true;
	Prepared = MoveTemp(Candidate);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareStructuresOnly(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	if (!ValidateConsumer(OutError))
	{
		return false;
	}

	FVoxelGenerationStructureKeyRange Range;
	if (!GetStructureKeyRange(InBounds, Range, OutError))
	{
		return false;
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FPreparedState Candidate;
	TGuardValue<const FPreparedState*> PreparingGuard(Preparing, &Candidate);
	const uint64 ExistingInputBytes = GetRequiredInputBytes() + RequiredOutputInputBytes;
	if (!Candidate.Reserve(Resources, Consumer, Range.Count, 0, true, ExistingInputBytes, GetRequiredInputLeaseCount() + RequiredOutputLeaseCount, OutError))
	{
		return false;
	}
	FVoxelGenerationBounds NaturalBounds = InBounds;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		NaturalBounds.Min[Axis] = int32(FMath::Clamp<int64>(int64(Range.Min[Axis]) * GenerationPlanTileSide,
			VoxelGeneration::WorldCellMin, VoxelGeneration::WorldCellMaxExclusive - 1));
		NaturalBounds.Max[Axis] = int32(FMath::Clamp<int64>((int64(Range.Max[Axis]) + 1) * GenerationPlanTileSide,
			VoxelGeneration::WorldCellMin + 1, VoxelGeneration::WorldCellMaxExclusive));
	}
	if (!RetainHydrologyReads(NaturalBounds, 0, Candidate.HydrologyRetention, OutError)) return false;
	for (int64 Y = Range.Min.Y; Y <= Range.Max.Y; ++Y)
	{
		for (int64 X = Range.Min.X; X <= Range.Max.X; ++X)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FVoxelGenerationTileKey Key{FIntVector(int32(X), int32(Y), 0)};
			FVoxelStructurePlanPtr StructurePlan;
			if (!EnsureStructurePlan(Key, StructurePlan, OutError, InCancel)) return false;
			Candidate.Structures.Add(StructurePlan);
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	Candidate.Bounds = InBounds;
	Candidate.Cancel = InCancel;
	Candidate.bColumnsPrepared = true;
	Prepared = MoveTemp(Candidate);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::GetBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_QueryBaseColumn);
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	if (!VoxelGeneration::IsWorldCell(InX) || !VoxelGeneration::IsWorldCell(InY))
	{
		OutError = TEXT("Invalid base column outside world cell domain");
		return false;
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	if (!bUseColumnCache)
	{
		if (!ComputeBaseColumn(
			InX,
			InY,
			OutColumn))
		{
			OutError = TEXT("Voxel base column generation failed");
			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelBaseColumnEntryPtr Entry;
	if (!Cache->GetOrBuildBaseColumn(
		FIntPoint(InX, InY),
		[this, InX, InY](FVoxelBaseColumnEntry& OutEntry, FString& BuildError)
		{
			OutEntry.Position = FIntPoint(InX, InY);
			if (!ComputeBaseColumn(InX, InY, OutEntry.Column))
			{
				BuildError = TEXT("Voxel base column generation failed");
				return false;
			}
			BuildError.Reset();
			return true;
		},
		Entry,
		OutError,
		InCancel) || !Entry)
	{
		return false;
	}

	OutColumn = Entry->Column;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ComputeBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ComputeBaseColumn);
	FVoxelColumnSample Column;

	Column.Climate =
		Climate->Sample(
			InX,
			InY);

	const FVoxelMacroTerrainSample TerrainSample =
		Terrain->SampleMacro(
			InX,
			InY);

	Column.SurfaceZ =
		TerrainSample.SurfaceZ;

	Column.DensityHeight =
		TerrainSample.DensityHeight;

	Column.SlopePermille =
		TerrainSample.SlopePermille;

	Column.Landform =
		TerrainSample.Landform;

	Column.BiomeIndex =
		Biome->Resolve(
			Column.Climate,
			TerrainSample);

	const int32 SeaLevel =
		Config->Recipe->
			Settings.SeaLevel;

	if (Column.SurfaceZ <=
		SeaLevel)
	{
		Column.SurfaceWaterZ =
			SeaLevel;

		Column.bOcean = true;
	}
	else
	{
		static const FIntPoint CoastNeighbors[] =
		{
			FIntPoint(-1, 0),
			FIntPoint(1, 0),
			FIntPoint(0, -1),
			FIntPoint(0, 1)
		};

		for (const FIntPoint& Offset :
			CoastNeighbors)
		{
			FIntPoint Neighbor;
			if (!VoxelGeneration::TryWorldPoint(FInt64Vector2(int64(InX) + Offset.X, int64(InY) + Offset.Y), Neighbor)) continue;
			if (Terrain->SampleMacro(
				Neighbor.X,
				Neighbor.Y).
				SurfaceZ <=
				SeaLevel)
			{
				Column.bCoast = true;
				break;
			}
		}
	}

	OutColumn =
		Column;

	return true;
}

bool FVoxelGenerationQuery::ComputeNaturalColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_NaturalColumn);

	FVoxelColumnSample Column;
	if (!GetBaseColumn(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}

	if (!ApplyHydrology(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}

	Column.DensityHeight = Column.SurfaceZ;
	Surface->ResolveColumn(InX, InY, Column);
	OutColumn = Column;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ApplyHydrology(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& InOutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_NaturalHydrology);
	const FVoxelHydrologyRegionKey Key = HydrologyKeyForVoxel(
		InX, InY, Config->Recipe->Settings);
	FVoxelHydrologyPlanPtr Plan;
	if (bRequireReadyHydrology && !Cache->FindHydrology(Key, Plan))
	{
		FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Hydrology,
			FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
		if (MissingHydrologyObserver) MissingHydrologyObserver(MissingHydrologyContext.Get(), Key);
		OutError = TEXT("DependencyNotReady: Hydrology");
		return false;
	}
	if (!EnsureHydrologyPlan(Key, Plan, OutError, InCancel) || !Plan)
	{
		return false;
	}

	FVoxelHydrologyInfluence Influence;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_HydrologyPlanSample);
		if (!Plan->Sample(InX, InY, InOutColumn.SurfaceZ, Influence))
		{
			OutError.Reset();
			return true;
		}
	}

	if (Influence.GroundOverrideZ != MIN_int32)
	{
		InOutColumn.SurfaceZ = Influence.GroundOverrideZ;
	}
	InOutColumn.SurfaceWaterZ = Influence.SurfaceWaterZ;
	InOutColumn.RiverDistanceCells = Influence.RiverDistanceCells;
	InOutColumn.BankDistanceCells = Influence.BankDistanceCells;
	InOutColumn.FloodplainStrengthQ15 = Influence.FloodplainStrengthQ15;
	InOutColumn.RiverId = Influence.RiverId;
	InOutColumn.RiverZone = Influence.RiverZone;
	InOutColumn.bRiver = Influence.bRiver;
	InOutColumn.bLake = Influence.bLake;
	InOutColumn.bOcean = Influence.bOcean;
	InOutColumn.bCoast = Influence.bCoast;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleEnvironmentColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!ValidateConsumer(OutError)) return false;
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), this, EnumerateQueryRequiredSources);
	FVoxelGenerationPlanningReadScope ConsumerScope(nullptr, nullptr, InCancel, Resources ? &Consumer : nullptr);
	if (!VoxelGeneration::IsWorldCell(InX) || !VoxelGeneration::IsWorldCell(InY))
	{
		OutError = TEXT("Invalid column outside world cell domain");
		return false;
	}
	if (!bUseColumnCache)
	{
		return ComputeNaturalColumn(
			InX,
			InY,
			OutColumn,
			OutError,
			InCancel);
	}

	FVoxelNaturalColumnEntryPtr Entry;
	if (!Cache->GetOrBuildNaturalColumn(
		FIntPoint(InX, InY),
		[this, InX, InY, InCancel](FVoxelNaturalColumnEntry& OutEntry, FString& BuildError)
		{
			OutEntry.Position = FIntPoint(InX, InY);
			return ComputeNaturalColumn(InX, InY, OutEntry.Column, BuildError, InCancel);
		},
		Entry,
		OutError,
		InCancel) || !Entry)
	{
		return false;
	}
	OutColumn = Entry->Column;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ResolveSurfaceCandidate(
	const int32 InX,
	const int32 InY,
	FVoxelSurfaceCandidate& OutCandidate,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelColumnSample Column;
	if (!SampleEnvironmentColumn(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}
	OutCandidate.XY = FIntPoint(InX, InY);
	OutCandidate.GroundZ = Column.SurfaceZ;
	OutCandidate.WaterZ = Column.SurfaceWaterZ;
	OutCandidate.SlopePermille = Column.SlopePermille;
	OutCandidate.BiomeIndex = Column.BiomeIndex;
	OutCandidate.bRiver = Column.bRiver;
	OutCandidate.bLake = Column.bLake;
	OutCandidate.bOcean = Column.bOcean;
	OutCandidate.bCoast = Column.bCoast;
	OutCandidate.bValid = OutCandidate.GroundZ >= Config->Recipe->Settings.MinZ &&
		int64(OutCandidate.GroundZ) < int64(Config->Recipe->Settings.MaxZ) - 2;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::IsInsidePreparedXY(
	const int32 InX,
	const int32 InY) const
{
	return
		InX >= Prepared.Bounds.Min.X &&
		InY >= Prepared.Bounds.Min.Y &&
		InX < Prepared.Bounds.Max.X &&
		InY < Prepared.Bounds.Max.Y;
}

TConstArrayView<FVoxelStructurePlanPtr>
FVoxelGenerationQuery::GetPreparedStructurePlans() const
{
	return Prepared.Structures;
}

bool FVoxelGenerationQuery::SampleColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError) const
{
	if (!Prepared.bColumnsPrepared)
	{
		OutError =
			TEXT("Voxel generation column query is not prepared");

		return false;
	}

	if (!SampleEnvironmentColumn(
		InX,
		InY,
		OutColumn,
		OutError,
		Prepared.Cancel))
	{
		return false;
	}

	const FVoxelAquiferSample Sample =
		Aquifer->Sample(
			FIntVector(
				InX,
				InY,
				OutColumn.SurfaceZ),
			OutColumn,
			0);

	OutColumn.GroundWaterZ =
		Sample.SurfaceZ;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleSymbol(
	const FIntVector& InPosition,
	uint32& OutValue,
	FString& OutError) const
{
	if (!Prepared.bSymbolsPrepared ||
		!Prepared.Bounds.Contains(
			InPosition))
	{
		OutError =
			TEXT("Voxel generation position is outside prepared symbol bounds");

		return false;
	}

	FVoxelColumnSample Column;

	if (!SampleEnvironmentColumn(
		InPosition.X,
		InPosition.Y,
		Column,
		OutError,
		Prepared.Cancel))
	{
		return false;
	}

	return ResolveSymbol(InPosition, Column, OutValue, OutError);
}

bool FVoxelGenerationQuery::SampleColumnSymbols(
	const FIntPoint& InColumn,
	const int32 InMinZ,
	const TArrayView<uint32> OutValues,
	FString& OutError) const
{
	const int32 InCount = OutValues.Num();
	if (!Prepared.bSymbolsPrepared || InCount <= 0 ||
		!Prepared.Bounds.Contains(FIntVector(InColumn.X, InColumn.Y, InMinZ)) ||
		static_cast<int64>(InMinZ) + InCount > Prepared.Bounds.Max.Z)
	{
		OutError = TEXT("Voxel symbol column is outside prepared bounds");
		return false;
	}
	FVoxelColumnSample Column;
	if (!SampleEnvironmentColumn(InColumn.X, InColumn.Y, Column, OutError, Prepared.Cancel))
	{
		return false;
	}
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		if (Prepared.Cancel && Prepared.Cancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (!ResolveSymbol(FIntVector(InColumn.X, InColumn.Y, InMinZ + Index), Column, OutValues[Index], OutError))
		{
			return false;
		}
	}
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FVoxelGenerationQuery::SampleColumnSymbols(const FIntPoint& InColumn, const int32 InMinZ,
	const int32 InCount, TArray<uint32>& OutValues, FString& OutError) const
{
	if (Resources)
	{
		OutError = TEXT("InvalidResource: bound generation query requires a caller-owned symbol span");
		return false;
	}
	if (InCount <= 0)
	{
		OutError = TEXT("Voxel symbol column count is invalid");
		return false;
	}
	TArray<uint32> Values;
	Values.SetNumUninitialized(InCount);
	if (!SampleColumnSymbols(InColumn, InMinZ, MakeArrayView(Values), OutError))
	{
		return false;
	}
	OutValues = MoveTemp(Values);
	return true;
}
#endif

bool FVoxelGenerationQuery::ResolveSymbol(const FIntVector& InPosition, FVoxelColumnSample Column,
	uint32& OutValue, FString& OutError) const
{
	uint32 Value =
		Config->Recipe->
			Palette.Air;

	static constexpr EVoxelGenerationStage Stages[] =
	{
		EVoxelGenerationStage::TerrainDensity,
		EVoxelGenerationStage::Aquifer,
		EVoxelGenerationStage::Surface,
		EVoxelGenerationStage::Carving,
		EVoxelGenerationStage::HydrologyRaster,
		EVoxelGenerationStage::BaseEcology,
		EVoxelGenerationStage::UndergroundStructures,
		EVoxelGenerationStage::SurfaceStructures,
		EVoxelGenerationStage::UndergroundOres,
		EVoxelGenerationStage::UndergroundDecoration,
		EVoxelGenerationStage::FluidFeatures,
		EVoxelGenerationStage::Vegetation,
		EVoxelGenerationStage::SurfaceDecoration,
		EVoxelGenerationStage::TopLayer
	};

	for (const EVoxelGenerationStage Stage :
		Stages)
	{
		if (!ApplyStage(
			Stage,
			InPosition,
			Column,
			Value,
			OutError))
		{
			return false;
		}
	}

	OutValue =
		Value;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ApplyStage(
	const EVoxelGenerationStage InStage,
	const FIntVector& InPosition,
	FVoxelColumnSample& InOutColumn,
	uint32& InOutValue,
	FString& OutError) const
{
	const FVoxelGenerationPalette& Palette =
		Config->Recipe->Palette;
	if (InStage == EVoxelGenerationStage::UndergroundDecoration)
	{
		auto Cavity = [&](const FIntVector& Position)
		{
			for (const FVoxelCavePlanPtr& Plan : Prepared.Caves) if (Plan && Plan->Carves(Position)) return true;
			return false;
		};
		auto Protected = [&](const FIntVector& Position)
		{
			for (const FVoxelCavePlanPtr& Plan : Prepared.Caves)
			{
				if (!Plan) continue;
				for (const FVoxelGenerationBounds& Bounds : Plan->ClearVolumes) if (Bounds.Contains(Position)) return true;
			}
			for (const FVoxelStructurePlanPtr& Plan : Prepared.Structures)
			{
				if (Plan && (Plan->IsCleared(Position, EVoxelGenerationStage::UndergroundStructures) || Plan->IsCleared(Position, EVoxelGenerationStage::SurfaceStructures))) return true;
			}
			return false;
		};
		InOutValue = FVoxelNaturalDecoration::Resolve(*Config->Recipe, InOutColumn, InPosition, InOutValue, Cavity, Protected);
	}

	switch (InStage)
	{
	case EVoxelGenerationStage::TerrainDensity:
	{
		const FVoxelMacroTerrainSample Macro {
			InOutColumn.SurfaceZ,
			InOutColumn.DensityHeight,
			InOutColumn.SlopePermille
		};

		InOutValue =
			Terrain->SampleDensityQ16(
				InPosition,
				Macro) > 0
				? Palette.Stone
				: Palette.Air;

		break;
	}

	case EVoxelGenerationStage::Aquifer:
		if ((InOutValue & 0xffffu) ==
			Palette.Air)
		{
			const FVoxelAquiferSample Sample =
				Aquifer->Sample(
					InPosition,
					InOutColumn,
					-1);

			if (Sample.Fluid ==
				EVoxelFluidKind::Water)
			{
				InOutValue =
					Palette.Water;
			}
			else if (
				Sample.Fluid ==
				EVoxelFluidKind::Lava)
			{
				InOutValue =
					Palette.Lava;
			}
		}
		break;

	case EVoxelGenerationStage::Surface:
		if ((InOutValue & 0xffffu) !=
				Palette.Air &&
			InPosition.Z <=
				InOutColumn.SurfaceZ)
		{
			InOutValue =
				Surface->ResolveSymbol(
					InPosition,
					InOutColumn,
					int64(InOutColumn.SurfaceZ) -
						InPosition.Z);
		}
		break;

	case EVoxelGenerationStage::Carving:
		for (const FVoxelCavePlanPtr& Plan :
			Prepared.Caves)
		{
			if (Plan &&
				Plan->Carves(
					InPosition))
			{
				InOutValue =
					Palette.Air;
				const bool bProtectedEntrance = Plan->ClearVolumes.ContainsByPredicate([&](const FVoxelGenerationBounds& Bounds) { return Bounds.Contains(InPosition); });
				if (!bProtectedEntrance)
				{
					const FVoxelAquiferSample Fluid = Aquifer->Sample(InPosition, InOutColumn, -1);
					if (Fluid.Fluid == EVoxelFluidKind::Water) InOutValue = Palette.Water;
					else if (Fluid.Fluid == EVoxelFluidKind::Lava) InOutValue = Palette.Lava;
				}

				break;
			}
		}
		break;

	case EVoxelGenerationStage::HydrologyRaster:
		if (InOutColumn.SurfaceWaterZ !=
				MIN_int32 &&
			InPosition.Z >
				InOutColumn.SurfaceZ &&
			InPosition.Z <=
				InOutColumn.SurfaceWaterZ)
		{
			InOutValue =
				Palette.Water;
		}
		break;

	case EVoxelGenerationStage::BaseEcology:
		if (static_cast<uint16>(InOutValue & 0xffffu) != Palette.Air)
		{
			break;
		}
		for (const FVoxelCavePlanPtr& Plan : Prepared.Caves)
		{
			if (Plan && Plan->Carves(InPosition))
			{
				return true;
			}
		}
		for (const FVoxelEcologyPlanPtr& Plan : Prepared.Ecology)
		{
			uint32 PlannedValue = 0;
			if (Plan && Plan->Sample(InPosition, PlannedValue))
			{
				InOutValue = PlannedValue;
				break;
			}
		}
		break;

	case EVoxelGenerationStage::UndergroundStructures:
	case EVoxelGenerationStage::SurfaceStructures:
		for (const FVoxelStructurePlanPtr& Plan :
			Prepared.Structures)
		{
			if (!Plan)
			{
				continue;
			}

			if (Plan->IsCleared(
				InPosition,
				InStage))
			{
				InOutValue =
					Palette.Air;
			}

			uint32 PlannedValue = 0;

			if (Plan->Sample(
				InPosition,
				InStage,
				PlannedValue))
			{
				InOutValue =
					PlannedValue;
			}
		}
		break;

	default:
		for (const FVoxelFeaturePlanPtr& Plan :
			Prepared.Features)
		{
			uint32 PlannedValue = 0;

			if (!Plan ||
				!Plan->Sample(
					InPosition,
					InStage,
					PlannedValue))
			{
				continue;
			}

			if ((InStage == EVoxelGenerationStage::Vegetation ||
				 InStage == EVoxelGenerationStage::SurfaceDecoration) &&
				static_cast<uint16>(InOutValue & 0xffffu) != Palette.Air)
			{
				continue;
			}

			InOutValue = PlannedValue;
		}
		break;
	}

	OutError.Reset();
	return true;
}
