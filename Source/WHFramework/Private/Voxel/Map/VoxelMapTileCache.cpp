#include "Voxel/Map/VoxelMapTileCache.h"

#include "Engine/Texture2D.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationCapacityWait.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

namespace
{
	struct FMapTilePixels : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TArray<FColor> Pixels;

		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FMapTilePixels>() + VoxelGenerationAllocation::AllocatedBytes(Pixels);
		}

		virtual uint64 GetResourceAccountedBytes() const override
		{
			return Lease ? Lease->Bytes() : 0;
		}
	};
}

struct FVoxelMapTileCache::FBuildWork
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> FactResources;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> OutputResources;
	TSharedPtr<FVoxelGenerationCapacityWait, ESPMode::ThreadSafe> CapacityWait;
	TSharedPtr<FVoxelGenerationCapacityWait, ESPMode::ThreadSafe> OutputCapacityWait;
	TSharedPtr<FMapTilePixels, ESPMode::ThreadSafe> Pixels;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelGenerationBounds Bounds;
	FIntPoint Origin;
	int32 Step = 0;
	TAtomic<int32> NextSample{0};
	TAtomic<int32> BatchEndSample{0};
	TAtomic<bool> bWaitingDependencies{true};
	TAtomic<bool> bApplyAllowed{true};

	EVoxelTaskBlockReason GetBlockReason() const
	{
		if (bWaitingDependencies.Load())
		{
			return EVoxelTaskBlockReason::Dependency;
		}
		return CapacityWait->BlockReason() | OutputCapacityWait->BlockReason();
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel, const FVoxelMapColorResolver& InResolver)
	{
		FVoxelTaskResult Result;
		Result.Kind = EVoxelTaskKind::BuildMapTile;
		if (InCancel.Load())
		{
			Result.bCanceled = true;
			return Result;
		}
		const uint64 BeforeFacts = CapacityWait->Begin();
		const uint64 BeforeOutput = OutputCapacityWait->Begin();
		if (!Pixels)
		{
			const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FMapTilePixels>() +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FColor, FDefaultAllocator>(TileSide * TileSide);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			const auto Admission = OutputResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				Result.Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
					? TEXT("CapacityBlocked: map tile pixel output")
					: Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
					? TEXT("CapacityUnachievable: map tile pixel output")
					: TEXT("InvalidRequest: map tile pixel output");
				OutputCapacityWait->Continue(Result, BeforeOutput);
				return Result;
			}
			Pixels = VoxelBoundedAllocation::MakeWorkerShared<FMapTilePixels>(Lease);
			Pixels->Lease = MoveTemp(Lease);
			Pixels->Pixels.Reserve(TileSide * TileSide);
			Pixels->Pixels.SetNumUninitialized(TileSide * TileSide);
			check(Pixels->GetAllocatedBytes() <= Pixels->Lease->Bytes());
		}
		FVoxelGenerationRequiredSourceScope RootSources(FactResources.Get(), Generator.Get(),
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				static_cast<const FVoxelGenerationPipeline*>(InContext)->VisitRequiredSources(InVisit);
			});
		FVoxelGenerationQuery Query;
		FVoxelGenerationRequiredSourceScope QuerySources(FactResources.Get(), &Query,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				static_cast<const FVoxelGenerationQuery*>(InContext)->VisitRequiredSources(InVisit);
			});
		if (!FVoxelGenerationQuery::Create(Config.ToSharedRef(), Generator->GetPlanCache(), Query,
			Result.Error, false, true, false, &Consumer) || !Query.PrepareColumns(Bounds, Result.Error, &InCancel))
		{
			Result.bCanceled = InCancel.Load();
			CapacityWait->Continue(Result, BeforeFacts);
			return Result;
		}
		int32 Cursor = NextSample.Load();
		const int32 End = BatchEndSample.Load();
		check(Cursor >= 0 && Cursor < End && End <= TileSide * TileSide);
		while (Cursor < End)
		{
			if (InCancel.Load())
			{
				NextSample.Store(Cursor);
				Result.bCanceled = true;
				return Result;
			}
			const int32 Column = Cursor % TileSide;
			const int32 Row = Cursor / TileSide;
			const int32 CellX = int32(int64(Origin.X) + int64(Column) * Step);
			const int32 CellY = int32(int64(Origin.Y) + int64(Row) * Step);
			FVoxelColumnSample Sample;
			if (!Query.SampleEnvironmentColumn(CellX, CellY, Sample, Result.Error, &InCancel))
			{
				NextSample.Store(Cursor);
				Result.bCanceled = InCancel.Load();
				CapacityWait->Continue(Result, BeforeFacts);
				return Result;
			}
			Pixels->Pixels[Column + (TileSide - 1 - Row) * TileSide] = InResolver(Sample, *Config);
			++Cursor;
		}
		NextSample.Store(Cursor);
		Result.bSuccess = true;
		if (Cursor == TileSide * TileSide)
		{
			Result.CustomPayload = Pixels;
		}
		else
		{
			bWaitingDependencies.Store(true);
			Result.bContinue = true;
		}
		return Result;
	}
};

FVoxelMapTileCache::FVoxelMapTileCache(
	FVoxelTaskScheduler& InScheduler,
	FVoxelEmergeManager& InEmergeManager,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const uint64 InWorldEpoch,
	const double InBlockSize)
	: Scheduler(InScheduler)
	, EmergeManager(InEmergeManager)
	, Generator(MoveTemp(InGenerator))
	, Config(MoveTemp(InConfig))
	, WorldEpoch(InWorldEpoch)
	, BlockSize(InBlockSize)
{
}

FVoxelMapTileCache::~FVoxelMapTileCache()
{
	ResetRequests();
}

int32 FVoxelMapTileCache::SelectStep(const float InCellPixelSize)
{
	for (const int32 Step : { 4, 8, 16, 32, 128 })
	{
		if (InCellPixelSize * Step >= 4.f)
		{
			return Step;
		}
	}
	return 512;
}

void FVoxelMapTileCache::VisibleTiles(
	const FSceneMapView& InView,
	const FVector2D& InPanelSize,
	const double InBlockSize,
	TArray<FVoxelMapTileKey>& OutKeys)
{
	OutKeys.Reset();
	if (!FMath::IsFinite(InView.Range) || InView.Range <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(InBlockSize) || InBlockSize <= 0.0 ||
		InPanelSize.X <= 0.0 || InPanelSize.Y <= 0.0)
	{
		return;
	}

	const double PixelsPerCell = InBlockSize * InPanelSize.X / InView.Range;
	const int32 Step = SelectStep(static_cast<float>(PixelsPerCell));
	const int64 TileCells = static_cast<int64>(TileSide) * Step;
	const double HalfX = InView.Range * 0.5 / InBlockSize;
	const double HalfY = HalfX * InPanelSize.Y / InPanelSize.X;
	const double Radius = FMath::Sqrt(HalfX * HalfX + HalfY * HalfY);
	const double CenterX = InView.Center.X / InBlockSize;
	const double CenterY = InView.Center.Y / InBlockSize;
	const int64 MinX = FMath::FloorToInt64((CenterX - Radius) / TileCells);
	const int64 MaxX = FMath::FloorToInt64((CenterX + Radius) / TileCells);
	const int64 MinY = FMath::FloorToInt64((CenterY - Radius) / TileCells);
	const int64 MaxY = FMath::FloorToInt64((CenterY + Radius) / TileCells);
	if (MaxX - MinX > 32 || MaxY - MinY > 32)
	{
		return;
	}
	for (int64 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int64 X = MinX; X <= MaxX; ++X)
		{
			const int64 OriginX = X * TileCells;
			const int64 OriginY = Y * TileCells;
			if (X >= MIN_int32 && X <= MAX_int32 &&
				Y >= MIN_int32 && Y <= MAX_int32 &&
				OriginX >= MIN_int32 && OriginY >= MIN_int32 &&
				OriginX + (TileSide - 1) * Step < MAX_int32 &&
				OriginY + (TileSide - 1) * Step < MAX_int32)
			{
				OutKeys.Add({ FIntPoint(static_cast<int32>(X), static_cast<int32>(Y)), Step });
			}
		}
	}
}

void FVoxelMapTileCache::Request(const FGuid& InSource, const TArray<FVoxelMapTileKey>& InKeys, const FSceneMapView& InView)
{
	check(IsInGameThread());
	if (bResetting || !ColorResolver || !InSource.IsValid())
	{
		return;
	}
	FViewRequest& View = Views.FindOrAdd(InSource);
	View.Keys = InKeys;
	View.View = InView;
	View.LastRequestFrame = GFrameCounter;
	RefreshRequests();
}

void FVoxelMapTileCache::ReleaseRequest(const FGuid& InSource)
{
	check(IsInGameThread());
	if (Views.Remove(InSource) && !bResetting)
	{
		RefreshRequests();
	}
}

void FVoxelMapTileCache::RefreshRequests()
{
	TSet<FVoxelMapTileKey> Targets;
	TSet<FVoxelMapTileKey> Desired;
	TArray<FVoxelMapTileKey> Requests;
	for (const auto& View : Views)
	{
		for (const FVoxelMapTileKey& Key : View.Value.Keys)
		{
			if (Key.Step <= 0 || Key.Step > MAX_int32 / (TileSide - 1))
			{
				continue;
			}
			Targets.Add(Key);
			if (!Desired.Contains(Key))
			{
				Desired.Add(Key);
				Requests.Add(Key);
			}
			if (FTile* Tile = Tiles.Find(Key))
			{
				Tile->LastUse = ++Sequence;
			}
			if (Key.Step < 512)
			{
				const FVoxelMapTileKey Parent = ParentKey(Key, 512);
				if (!Desired.Contains(Parent))
				{
					Desired.Add(Parent);
					Requests.Add(Parent);
				}
			}
		}
	}
	TArray<FVoxelMapTileKey> Retiring;
	for (const auto& Pair : Pending)
	{
		if (!Desired.Contains(Pair.Key))
		{
			Retiring.Add(Pair.Key);
		}
	}
	for (const auto& Key : Retiring)
	{
		CancelRequest(Key);
	}
	const auto DistanceSquared = [this](const FVoxelMapTileKey& InKey)
	{
		const FVector2D Center = (FVector2D(InKey.Coordinate) + FVector2D(0.5)) * TileSide * InKey.Step;
		double Distance = MAX_dbl;
		for (const auto& View : Views)
		{
			Distance = FMath::Min(Distance, FVector2D::DistSquared(Center, View.Value.View.Center / BlockSize));
		}
		return Distance;
	};
	Requests.Sort([&Targets, &DistanceSquared](const FVoxelMapTileKey& Left, const FVoxelMapTileKey& Right)
	{
		const bool bLeftTarget = Targets.Contains(Left);
		const bool bRightTarget = Targets.Contains(Right);
		if (bLeftTarget != bRightTarget)
		{
			return bLeftTarget;
		}
		const double LeftDistance = DistanceSquared(Left);
		const double RightDistance = DistanceSquared(Right);
		if (LeftDistance != RightDistance)
		{
			return LeftDistance < RightDistance;
		}
		if (Left.Step != Right.Step)
		{
			return Left.Step < Right.Step;
		}
		return Left.Coordinate.X != Right.Coordinate.X
			? Left.Coordinate.X < Right.Coordinate.X : Left.Coordinate.Y < Right.Coordinate.Y;
	});
	int32 Active = 0;
	for (const auto& Pair : Pending)
	{
		Active += Pair.Value.bFailed ? 0 : 1;
	}
	int32 Advanced = 0;
	const double Deadline = FPlatformTime::Seconds() + 0.0005;
	for (const FVoxelMapTileKey& Key : Requests)
	{
		if (Advanced == 4 || FPlatformTime::Seconds() >= Deadline)
		{
			break;
		}
		if (Tiles.Contains(Key))
		{
			continue;
		}
		if (!Pending.Contains(Key))
		{
			if (Active == 8)
			{
				continue;
			}
			FString Error;
			if (!ReservePendingStorage(Pending.Num() + 1, Error))
			{
				break;
			}
			const int64 TileCells = int64(TileSide) * Key.Step;
			const int64 OriginX = int64(Key.Coordinate.X) * TileCells;
			const int64 OriginY = int64(Key.Coordinate.Y) * TileCells;
			const int64 MaxX = OriginX + int64(TileSide - 1) * Key.Step + 1;
			const int64 MaxY = OriginY + int64(TileSide - 1) * Key.Step + 1;
			if (Key.Step <= 0 || OriginX < VoxelGeneration::WorldCellMin || OriginY < VoxelGeneration::WorldCellMin ||
				MaxX > VoxelGeneration::WorldCellMaxExclusive || MaxY > VoxelGeneration::WorldCellMaxExclusive)
			{
				continue;
			}
			check(Sequence != MAX_uint64);
			FPendingTile Candidate;
			Candidate.Stamp.WorldEpoch = WorldEpoch;
			Candidate.Stamp.OwnerId = GenerationOwnerId;
			Candidate.Stamp.OwnerIncarnation = StyleEpoch + 1;
			Candidate.Stamp.Token = ++Sequence;
			Candidate.Consumer = {EVoxelGenerationConsumerKind::Environment, WorldEpoch, GenerationOwnerId,
				StyleEpoch + 1, EVoxelGenerationSpatialKind::Bounds,
				FIntVector(int32(OriginX), int32(OriginY), Config->Recipe->Settings.MinZ),
				FIntVector(int32(MaxX), int32(MaxY), Config->Recipe->Settings.MaxZ), Candidate.Stamp.Token};
			Pending.Add(Key, MoveTemp(Candidate));
			PendingStorageLease->MarkPublished();
			++Active;
			++Advanced;
		}
		Pending.FindChecked(Key).DistanceScore = FMath::Sqrt(DistanceSquared(Key));
	}
}

void FVoxelMapTileCache::Tick(const double InDeadline)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MapTileAdmissions);
	if (bResetting) return;
	bool bViewsExpired = false;
	for (auto View = Views.CreateIterator(); View; ++View)
	{
		if (GFrameCounter >= View.Value().LastRequestFrame && GFrameCounter - View.Value().LastRequestFrame > 2)
		{
			View.RemoveCurrent();
			bViewsExpired = true;
		}
	}
	if (bViewsExpired) RefreshRequests();
	TArray<FVoxelMapTileKey, TInlineAllocator<8>> Candidates;
	for (const auto& Pair : Pending)
	{
		if (!Pair.Value.bFailed)
		{
			Candidates.Add(Pair.Key);
		}
	}
	check(Candidates.Num() <= 8);
	Candidates.Sort([this](const FVoxelMapTileKey& Left, const FVoxelMapTileKey& Right)
	{
		const auto& LeftState = Pending.FindChecked(Left);
		const auto& RightState = Pending.FindChecked(Right);
		return LeftState.DistanceScore != RightState.DistanceScore
			? LeftState.DistanceScore < RightState.DistanceScore : LeftState.Stamp.Token < RightState.Stamp.Token;
	});
	int32 Advanced = 0;
	const double Deadline = FMath::Min(InDeadline, FPlatformTime::Seconds() + 0.0005);
	for (const auto& Key : Candidates)
	{
		if (Advanced == 4 || FPlatformTime::Seconds() >= Deadline)
		{
			break;
		}
		const FPendingTile* Current = Pending.Find(Key);
		if (Current && AdvanceRequest(Key, Current->DistanceScore))
		{
			++Advanced;
		}
	}
}

bool FVoxelMapTileCache::ReservePendingStorage(const int32 InRequired, FString& OutError)
{
	if (InRequired <= PendingCapacity)
	{
		return true;
	}
	if (InRequired <= 0 || PendingCapacity > MAX_int32 / 2)
	{
		OutError = TEXT("Oversized: map request directory");
		return false;
	}
	const int32 Capacity = FMath::Max(InRequired, FMath::Max(8, PendingCapacity * 2));
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelMapTileKey, FPendingTile>>(Pending, Capacity);
	const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::Environment, WorldEpoch, GenerationOwnerId,
		StyleEpoch + 1, EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = Scheduler.GetTerrainResources()->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes,
		Lease, 0, PendingStorageLease ? PendingStorageLease->Bytes() : 0, PendingStorageLease ? 1 : 0);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
			? TEXT("CapacityBlocked: map request directory") : TEXT("CapacityUnachievable: map request directory");
		return false;
	}
	Pending.Reserve(Capacity);
	check(VoxelGenerationAllocation::AllocatedBytes(Pending) <= Lease->Bytes());
	PendingStorageLease = MoveTemp(Lease);
	PendingCapacity = Capacity;
	return true;
}

int32 FVoxelMapTileCache::SelectBatchRows(const FVoxelMapTileKey& InKey, const FIntPoint& InOrigin,
	const int32 InStartRow) const
{
	const auto& Settings = Config->Recipe->Settings;
	int32 UniqueColumns = 0;
	int32 PreviousX = 0;
	for (int32 Column = 0; Column < TileSide; ++Column)
	{
		const int32 CellX = int32(int64(InOrigin.X) + int64(Column) * InKey.Step);
		const int32 RegionX = FVoxelGenerationQuery::HydrologyKeyForVoxel(CellX, InOrigin.Y, Settings, Config->Recipe->AlgorithmVersion).Coordinate.X;
		if (Column == 0 || RegionX != PreviousX)
		{
			++UniqueColumns;
			PreviousX = RegionX;
		}
	}
	int32 Rows = 0;
	int32 UniqueRows = 0;
	int32 PreviousY = 0;
	for (int32 Row = InStartRow; Row < TileSide; ++Row)
	{
		const int32 CellY = int32(int64(InOrigin.Y) + int64(Row) * InKey.Step);
		const int32 RegionY = FVoxelGenerationQuery::HydrologyKeyForVoxel(InOrigin.X, CellY, Settings, Config->Recipe->AlgorithmVersion).Coordinate.Y;
		if (Row == InStartRow || RegionY != PreviousY)
		{
			if ((UniqueRows + 1) * UniqueColumns > 64)
			{
				break;
			}
			++UniqueRows;
			PreviousY = RegionY;
		}
		++Rows;
	}
	check(Rows > 0);
	return Rows;
}

bool FVoxelMapTileCache::DeferRequest(const FVoxelMapTileKey& InKey,
	const FVoxelGenerationConsumerKey& InConsumer, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const uint64 InBeforeAdmission, const FString& InError)
{
	FPendingTile* Current = Pending.Find(InKey);
	if (!Current || Current->Consumer != InConsumer)
	{
		return true;
	}
	if (InError.StartsWith(TEXT("CapacityBlocked")))
	{
		Current->AdmissionResources = InResources;
		Current->CapacityVersion = InBeforeAdmission;
		Current->bWaitingCapacity = true;
		Current->bWakePending = true;
		return true;
	}
	const FVoxelTaskStamp Stamp = Current->Stamp;
	Current->bFailed = true;
	Current->bWaitingCapacity = false;
	if (Current->Work) Current->Work->bApplyAllowed.Store(false);
	Current->Request.Reset();
	Current->Work.Reset();
	Current->AdmissionResources.Reset();
	Scheduler.CancelMatching([Stamp](const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp)
	{
		return InKind == EVoxelTaskKind::BuildMapTile && InStamp == Stamp;
	});
	EmergeManager.ReleaseGenerationFacts(InConsumer);
	UE_LOG(LogTemp, Warning, TEXT("Voxel map tile %d,%d step %d serial=%llu: %s"),
		InKey.Coordinate.X, InKey.Coordinate.Y, InKey.Step, InConsumer.RequestSerial, *InError);
	return true;
}

bool FVoxelMapTileCache::AdvanceRequest(const FVoxelMapTileKey& InKey, const double InDistanceScore)
{
	FPendingTile* Current = Pending.Find(InKey);
	if (!Current || Current->bFailed)
	{
		return false;
	}
	const auto Consumer = Current->Consumer;
	if (Current->bWaitingCapacity && Current->AdmissionResources->ReleaseVersionExcludingOwner(Consumer) <= Current->CapacityVersion)
	{
		return false;
	}
	Current->bWaitingCapacity = false;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto FactResources = Scheduler.GetRuntimeResources();
	uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Current->Work)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FBuildWork>(), Lease, 0, PendingStorageLease->Bytes(), 1);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			return DeferRequest(InKey, Consumer, Resources, BeforeAdmission,
				Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
				? TEXT("CapacityBlocked: map worker") : TEXT("CapacityUnachievable: map worker"));
		}
		Current->Work = VoxelBoundedAllocation::MakeWorkerShared<FBuildWork>(Lease);
		Current->Work->MetadataLease = MoveTemp(Lease);
		Current->Work->Generator = Generator;
		Current->Work->Config = Config;
		Current->Work->FactResources = FactResources;
		Current->Work->OutputResources = Resources;
		Current->Work->Consumer = Consumer;
		Current->Work->Bounds = {Consumer.Min, Consumer.Max};
		Current->Work->Origin = FIntPoint(Consumer.Min.X, Consumer.Min.Y);
		Current->Work->Step = InKey.Step;
	}
	const auto Work = Current->Work;
	if (!Work->CapacityWait)
	{
		const uint64 BeforeFacts = FactResources->ReleaseVersionExcludingOwner(Consumer);
		Work->CapacityWait = FVoxelGenerationCapacityWait::Create(FactResources, Consumer);
		if (!Work->CapacityWait)
		{
			return DeferRequest(InKey, Consumer, FactResources, BeforeFacts, TEXT("CapacityBlocked: map fact wait"));
		}
	}
	if (!Work->OutputCapacityWait)
	{
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		Work->OutputCapacityWait = FVoxelGenerationCapacityWait::Create(Resources, Consumer);
		if (!Work->OutputCapacityWait)
		{
			return DeferRequest(InKey, Consumer, Resources, BeforeAdmission, TEXT("CapacityBlocked: map output wait"));
		}
	}
	if (Work->bWaitingDependencies.Load())
	{
		const int32 StartRow = Work->NextSample.Load() / TileSide;
		if (Current->PreflightStartRow != StartRow)
		{
			Current->PreflightStartRow = StartRow;
			Current->PreflightRows = SelectBatchRows(InKey, Work->Origin, StartRow);
			Current->bWakePending = true;
		}
		if (!Current->bWakePending)
		{
			return false;
		}
		Current->bWakePending = false;
		const int32 Rows = Current->PreflightRows;
		const FIntPoint BatchOrigin(Work->Origin.X, int32(int64(Work->Origin.Y) + int64(StartRow) * InKey.Step));
		bool bNeedsAdvance = false;
		const uint64 BeforeFacts = FactResources->ReleaseVersionExcludingOwner(Consumer);
		const auto Status = EmergeManager.EnsureEnvironmentSamples(Consumer, BatchOrigin,
			TileSide, Rows, InKey.Step, InDistanceScore, bNeedsAdvance);
		Current = Pending.Find(InKey);
		if (!Current || Current->Consumer != Consumer)
		{
			return true;
		}
		if (Status.bFailed || Status.Error.StartsWith(TEXT("CapacityBlocked")))
		{
			return DeferRequest(InKey, Consumer, FactResources, BeforeFacts, Status.Error);
		}
		Current->bWakePending = bNeedsAdvance;
		if (!Status.bReady)
		{
			return true;
		}
		Work->BatchEndSample.Store((StartRow + Rows) * TileSide);
		Work->bWaitingDependencies.Store(false);
		if (Current->bSubmitted)
		{
			return true;
		}
	}
	if (Current->bSubmitted)
	{
		return false;
	}
	if (!Current->Request)
	{
		Current->Request.Emplace();
		Current->Request->Kind = EVoxelTaskKind::BuildMapTile;
		Current->Request->WorkClass = EVoxelWorkClass::Background;
		Current->Request->Stamp = Current->Stamp;
		Current->Request->GenerationConsumer = Consumer;
		Current->Request->ReservedBytes = 1;
		Current->Request->bResumable = true;
	}
	auto& Request = Current->Request.GetValue();
	Request.DistanceScore = InDistanceScore;
	Request.InputBytes = Work->MetadataLease->Bytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationCapacityWait>();
	Request.SetRequiredBindingInput(PendingStorageLease->Bytes() + Request.InputBytes, 3);
	FString Error;
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Request.bBlockBound && !Request.BindGetBlockReason(Resources, Consumer,
		[Work] { return Work->GetBlockReason(); }, Error))
	{
		return DeferRequest(InKey, Consumer, Resources, BeforeAdmission, Error);
	}
	if (!Request.bExecuteBound)
	{
		const FVoxelMapColorResolver WorkerResolver = ColorResolver;
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if (!Request.BindExecute(Resources, Consumer,
			[Work, WorkerResolver](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel, WorkerResolver); }, Error))
		{
			return DeferRequest(InKey, Consumer, Resources, BeforeAdmission, Error);
		}
	}
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Request.bApplyBound && !Request.BindApply(Resources, Consumer,
		[this, InKey, Consumer, Work](FVoxelTaskResult&& Result)
		{
			if (!Work->bApplyAllowed.Load()) return;
			const FPendingTile* Current = Pending.Find(InKey);
			if (Current && Current->Consumer == Consumer && Current->Stamp == Result.Stamp && !Current->bFailed)
			{
				ApplyTile(InKey, MoveTemp(Result));
			}
		}, Error))
	{
		return DeferRequest(InKey, Consumer, Resources, BeforeAdmission, Error);
	}
	const FVoxelTaskStamp Stamp = Current->Stamp;
	FVoxelTaskRequest Submitted = MoveTemp(Request);
	Current->Request.Reset();
	Current->bSubmitted = true;
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const bool bEnqueued = Scheduler.Enqueue(MoveTemp(Submitted));
	Current = Pending.Find(InKey);
	if (!Current || Current->Consumer != Consumer || Current->Stamp != Stamp)
	{
		return true;
	}
	if (bEnqueued)
	{
		Current->AdmissionResources.Reset();
		return true;
	}
	Current->bSubmitted = false;
	Current->Request.Emplace(MoveTemp(Submitted));
	const auto Reason = Current->Request->Blocked;
	if (Reason == EVoxelTaskBlockReason::Oversized || Reason == EVoxelTaskBlockReason::TerminalFailure)
	{
		return DeferRequest(InKey, Consumer, Resources, BeforeAdmission,
			Reason == EVoxelTaskBlockReason::Oversized ? TEXT("CapacityUnachievable: map task admission")
			: TEXT("InvalidRequest: map task admission"));
	}
	if (Reason == EVoxelTaskBlockReason::InputMemory || Reason == EVoxelTaskBlockReason::Memory)
	{
		return DeferRequest(InKey, Consumer, Resources, BeforeAdmission, TEXT("CapacityBlocked: map task admission"));
	}
	return true;
}

void FVoxelMapTileCache::OnGenerationDependencyReady(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (InConsumer.WorldEpoch != WorldEpoch || InConsumer.OwnerId != GenerationOwnerId ||
		InConsumer.OwnerIncarnation != StyleEpoch + 1)
	{
		return;
	}
	for (auto& Pair : Pending)
	{
		if (Pair.Value.Consumer == InConsumer && !Pair.Value.bFailed)
		{
			Pair.Value.bWakePending = true;
			return;
		}
	}
}

void FVoxelMapTileCache::CancelRequest(const FVoxelMapTileKey& InKey)
{
	FPendingTile* Current = Pending.Find(InKey);
	if (!Current)
	{
		return;
	}
	const auto Consumer = Current->Consumer;
	const auto Stamp = Current->Stamp;
	if (Current->Work) Current->Work->bApplyAllowed.Store(false);
	Pending.Remove(InKey);
	Scheduler.CancelMatching([Stamp](const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp)
	{
		return InKind == EVoxelTaskKind::BuildMapTile && InStamp == Stamp;
	});
	EmergeManager.ReleaseGenerationFacts(Consumer);
	if (Pending.IsEmpty())
	{
		Pending.Empty();
		PendingStorageLease.Reset();
		PendingCapacity = 0;
	}
}

void FVoxelMapTileCache::ResetRequests()
{
	check(IsInGameThread());
	TGuardValue<bool> ResetGuard(bResetting, true);
	const uint64 Incarnation = StyleEpoch + 1;
	auto RetiringStorageLease = MoveTemp(PendingStorageLease);
	auto Retiring = MoveTemp(Pending);
	PendingCapacity = 0;
	for (const auto& Pair : Retiring)
	{
		if (Pair.Value.Work) Pair.Value.Work->bApplyAllowed.Store(false);
	}
	Scheduler.CancelMatching([Owner = GenerationOwnerId, Incarnation](const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp)
	{
		return InKind == EVoxelTaskKind::BuildMapTile && InStamp.OwnerId == Owner && InStamp.OwnerIncarnation == Incarnation;
	});
	for (const auto& Pair : Retiring)
	{
		EmergeManager.ReleaseGenerationFacts(Pair.Value.Consumer);
	}
	Retiring.Empty();
	RetiringStorageLease.Reset();
}

const FSlateBrush* FVoxelMapTileCache::FindBrush(const FVoxelMapTileKey& InKey) const
{
	const FTile* Tile = Tiles.Find(InKey);
	return Tile ? &Tile->Brush : nullptr;
}

void FVoxelMapTileCache::SetColorResolver(FVoxelMapColorResolver InResolver, const uint32 InStyleRevision)
{
	check(IsInGameThread());
	if (ColorResolver && StyleRevision == InStyleRevision)
	{
		return;
	}
	ColorResolver = MoveTemp(InResolver);
	StyleRevision = InStyleRevision;
	ResetRequests();
	++StyleEpoch;
	Tiles.Reset();
	++Revision;
}

FVoxelMapTileKey FVoxelMapTileCache::ParentKey(const FVoxelMapTileKey& InKey, const int32 InParentStep)
{
	const int64 ParentCells = static_cast<int64>(TileSide) * InParentStep;
	return { FIntPoint(
		static_cast<int32>(FMath::FloorToInt64(static_cast<double>(InKey.Coordinate.X) * TileSide * InKey.Step / ParentCells)),
		static_cast<int32>(FMath::FloorToInt64(static_cast<double>(InKey.Coordinate.Y) * TileSide * InKey.Step / ParentCells))), InParentStep };
}

FBox2f FVoxelMapTileCache::ParentUV(const FVoxelMapTileKey& InKey, const FVoxelMapTileKey& InParent)
{
	const double Scale = static_cast<double>(InKey.Step) / InParent.Step;
	const double MinX = static_cast<double>(InKey.Coordinate.X) * Scale - InParent.Coordinate.X;
	const double MinY = static_cast<double>(InKey.Coordinate.Y) * Scale - InParent.Coordinate.Y;
	return FBox2f(FVector2f(MinX, 1.0 - MinY - Scale), FVector2f(MinX + Scale, 1.0 - MinY));
}

bool FVoxelMapTileCache::FindDrawBrush(const FVoxelMapTileKey& InKey, FSlateBrush& OutBrush) const
{
	if (const FSlateBrush* Brush = FindBrush(InKey))
	{
		OutBrush = *Brush;
		return true;
	}
	for (const int32 Step : { 8, 16, 32, 128, 512 })
	{
		if (Step <= InKey.Step)
		{
			continue;
		}
		const FVoxelMapTileKey Parent = ParentKey(InKey, Step);
		if (const FSlateBrush* Brush = FindBrush(Parent))
		{
			OutBrush = *Brush;
			OutBrush.SetUVRegion(ParentUV(InKey, Parent));
			return true;
		}
	}
	return false;
}

void FVoxelMapTileCache::ApplyTile(const FVoxelMapTileKey& InKey, FVoxelTaskResult&& InResult)
{
	check(IsInGameThread());
	const FPendingTile* Current = Pending.Find(InKey);
	if (!Current || Current->Stamp != InResult.Stamp)
	{
		return;
	}
	const auto Consumer = Current->Consumer;
	if (InResult.bCanceled)
	{
		CancelRequest(InKey);
		return;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	if (!InResult.bSuccess || !InResult.CustomPayload)
	{
		DeferRequest(InKey, Consumer, Resources, 0,
			InResult.Error.IsEmpty() ? TEXT("InvalidResource: map task completed without pixels") : InResult.Error);
		return;
	}
	const FMapTilePixels* Pixels = static_cast<const FMapTilePixels*>(InResult.CustomPayload.Get());
	if (Pixels->Pixels.Num() != TileSide * TileSide)
	{
		DeferRequest(InKey, Consumer, Resources, 0, TEXT("InvalidResource: incomplete map pixel output"));
		return;
	}
	FTile Tile;
	Tile.Texture.Reset(UTexture2D::CreateTransient(TileSide, TileSide, PF_B8G8R8A8));
	if (!Tile.Texture)
	{
		DeferRequest(InKey, Consumer, Resources, 0, TEXT("InvalidResource: map texture creation failed"));
		return;
	}
	Tile.Texture->SRGB = true;
	Tile.Texture->Filter = TF_Nearest;
	Tile.Texture->NeverStream = true;
	void* Buffer = Tile.Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Buffer, Pixels->Pixels.GetData(), Pixels->Pixels.Num() * sizeof(FColor));
	Tile.Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Tile.Texture->UpdateResource();
	Tile.Brush.SetResourceObject(Tile.Texture.Get());
	Tile.Brush.ImageSize = FVector2D(TileSide, TileSide);
	Tile.LastUse = ++Sequence;
	Tiles.Add(InKey, MoveTemp(Tile));
	InResult.MarkWorkUsed();
	EmergeManager.ReleaseGenerationFacts(Consumer);
	Pending.Remove(InKey);
	if (Pending.IsEmpty())
	{
		Pending.Empty();
		PendingStorageLease.Reset();
		PendingCapacity = 0;
	}
	while (Tiles.Num() > 64)
	{
		EvictOldest();
	}
	++Revision;
}

void FVoxelMapTileCache::EvictOldest()
{
	const FVoxelMapTileKey* Oldest = nullptr;
	uint64 OldestUse = MAX_uint64;
	for (const TPair<FVoxelMapTileKey, FTile>& Pair : Tiles)
	{
		if (Pair.Value.LastUse < OldestUse)
		{
			Oldest = &Pair.Key;
			OldestUse = Pair.Value.LastUse;
		}
	}
	if (Oldest)
	{
		Tiles.Remove(*Oldest);
	}
}
