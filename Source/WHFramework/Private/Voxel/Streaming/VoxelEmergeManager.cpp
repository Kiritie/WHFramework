#include "Voxel/Streaming/VoxelEmergeManager.h"

#include "EngineGlobals.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"


struct FVoxelEmergeManager::FInputBudget
{
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease;
	EVoxelTerrainResourceAdmission& Admission;
	uint64& BeforeAdmission;

	FInputBudget(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease,
		EVoxelTerrainResourceAdmission& InAdmission, uint64& InBeforeAdmission)
		: Resources(InResources), Consumer(InConsumer), Lease(InLease), Admission(InAdmission), BeforeAdmission(InBeforeAdmission)
	{
	}

	bool Check(FString& OutError)
	{
		if (!Consumer.IsValid() || (Lease && !Lease->IsOwnedBy(*Resources)))
		{
			return Fail(TEXT("InvalidRequest: emerge admission storage domain"), OutError);
		}
		return true;
	}

	bool Fail(const TCHAR* InError, FString& OutError)
	{
		OutError = InError;
		Admission = OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelTerrainResourceAdmission::CapacityBlocked :
			OutError.StartsWith(TEXT("Oversized")) || OutError.StartsWith(TEXT("CapacityUnachievable")) ?
			EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}

	bool PrepareBytes(const uint64 InOldBytes, const uint64 InNewBytes, FString& OutError)
	{
		const uint64 Current = Lease ? Lease->Bytes() : 0;
		if (InOldBytes > Current || InNewBytes > MAX_uint64 - Current)
		{
			return Fail(TEXT("Oversized: emerge admission backing peak"), OutError);
		}
		FVoxelGenerationRequiredCapacity Sources;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Lease.Get(), Sources, OutError))
		{
			return Fail(*OutError, OutError);
		}
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		Admission = Lease ? Lease->TryResize(Current + InNewBytes,
			Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount) :
			Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Current + InNewBytes, Lease, 0,
				Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: emerge admission input peak") :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: emerge admission input peak") :
				TEXT("InvalidRequest: emerge admission input peak");
			return false;
		}
		return true;
	}

	bool SettleBytes(const uint64 InOldBytes, const uint64 InReservedBytes, const uint64 InActualBytes, FString& OutError)
	{
		const uint64 Current = Lease ? Lease->Bytes() : 0;
		if (InActualBytes > InReservedBytes || InReservedBytes > Current || InOldBytes > Current - InReservedBytes)
		{
			return Fail(TEXT("AllocationAccountingMismatch: emerge admission allocator"), OutError);
		}
		const bool bSettled = Lease->Resize(Current - InReservedBytes - InOldBytes + InActualBytes);
		check(bSettled);
		Admission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	static uint64 SparseAllocatedBytes(const TSparseArray<FIntVector>& InArray)
	{
		VoxelGenerationAllocation::Private::FSetCapacityArchive Capacity;
		InArray.CountBytes(Capacity);
		check(Capacity.Count == 2);
		using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FIntVector), alignof(FIntVector)>>;
		const uint64 Data = Capacity.MaxBytes[0] ? FMemory::QuantizeSize(Capacity.MaxBytes[0], alignof(FStorage)) : 0;
		const uint64 Flags = Capacity.MaxBytes[1] > 4 * sizeof(uint32) ? FMemory::QuantizeSize(Capacity.MaxBytes[1], DEFAULT_ALIGNMENT) : 0;
		return Data + Flags;
	}

	static bool SparseCanReserve(const TSparseArray<FIntVector>& InArray, const int32 InCapacity)
	{
		VoxelGenerationAllocation::Private::FSetCapacityArchive Capacity;
		InArray.CountBytes(Capacity);
		check(Capacity.Count == 2);
		using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FIntVector), alignof(FIntVector)>>;
		return InCapacity >= 0 && uint64(InCapacity) * sizeof(FStorage) <= Capacity.MaxBytes[0] &&
			((uint64(InCapacity) + 31) / 32) * sizeof(uint32) <= Capacity.MaxBytes[1];
	}

	bool ReserveSparse(TSparseArray<FIntVector>& InArray, const int32 InCapacity, FString& OutError)
	{
		if (!Check(OutError))
		{
			return false;
		}
		if (InCapacity < 0)
		{
			return Fail(TEXT("InvalidRequest: external data sparse capacity"), OutError);
		}
		if (SparseCanReserve(InArray, InCapacity))
		{
			InArray.Reserve(InCapacity);
			return true;
		}
		VoxelGenerationAllocation::Private::FSetCapacityArchive Capacity;
		InArray.CountBytes(Capacity);
		check(Capacity.Count == 2);
		using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FIntVector), alignof(FIntVector)>>;
		const uint64 Data = FMath::Max(Capacity.MaxBytes[0] ? uint64(FMemory::QuantizeSize(Capacity.MaxBytes[0], alignof(FStorage))) : 0,
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FStorage, FDefaultAllocator>(InCapacity));
		const int32 Words = int32((uint64(InCapacity) + 31) / 32);
		const int32 OldWords = int32(Capacity.MaxBytes[1] / sizeof(uint32));
		FDefaultBitArrayAllocator::ForElementType<uint32> Allocator;
		const uint64 FlagRaw = InArray.GetMaxIndex() == 0 ? FMath::Max(Capacity.MaxBytes[1], uint64(FMath::Max(4, Words)) * sizeof(uint32)) :
			Words > OldWords ? uint64(Allocator.CalculateSlackGrow(Words, OldWords, sizeof(uint32))) * sizeof(uint32) : Capacity.MaxBytes[1];
		if (FlagRaw / sizeof(uint32) > uint64(MAX_int32) / 32)
		{
			return Fail(TEXT("Oversized: external data sparse bitmap capacity"), OutError);
		}
		const uint64 Flags = FlagRaw > 4 * sizeof(uint32) ? FMemory::QuantizeSize(FlagRaw, DEFAULT_ALIGNMENT) : 0;
		const uint64 OldBytes = SparseAllocatedBytes(InArray);
		const uint64 NewBytes = Data + Flags;
		if (!PrepareBytes(OldBytes, NewBytes, OutError))
		{
			return false;
		}
		InArray.Reserve(InCapacity);
		return SettleBytes(OldBytes, NewBytes, SparseAllocatedBytes(InArray), OutError);
	}

};

const FIntVector& FVoxelEmergeManager::FCriticalReadyQueue::At(const int32 InIndex) const
{
	check(InIndex >= 0 && InIndex < Count);
	return Values[int32((int64(Head) + InIndex) % Values.Num())];
}

bool FVoxelEmergeManager::FCriticalReadyQueue::Peek(FIntVector& OutKey) const
{
	if (Count == 0)
	{
		return false;
	}
	OutKey = At(0);
	return true;
}

bool FVoxelEmergeManager::FCriticalReadyQueue::Dequeue(FIntVector& OutKey)
{
	if (!Peek(OutKey))
	{
		return false;
	}
	Head = (Head + 1) % Values.Num();
	--Count;
	return true;
}

bool FVoxelEmergeManager::FCriticalReadyQueue::Enqueue(const FIntVector& InKey)
{
	if (Count >= Values.Num())
	{
		return false;
	}
	Values[int32((int64(Head) + Count) % Values.Num())] = InKey;
	++Count;
	return true;
}

void FVoxelEmergeManager::FCriticalReadyQueue::Empty()
{
	Head = 0;
	Count = 0;
}

FVoxelGenerationConsumerKey FVoxelEmergeManager::GetAdmissionConsumer() const
{
	return FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), GenerationOwnerId, 1, {}, 1);
}

bool FVoxelEmergeManager::CanRetryStorage(FStorageAdmission& InAdmission, const FVoxelTerrainResources& InResources,
	const TCHAR* InError, FString& OutError) const
{
	if (InAdmission.bWaitingCapacity &&
		InResources.ReleaseVersionExcludingOwner(GetAdmissionConsumer()) <= InAdmission.CapacityVersion)
	{
		OutError = InError;
		return false;
	}
	return true;
}

bool FVoxelEmergeManager::DeferTickAdmission(const uint64 InBeforeAdmission, const FString& InError)
{
	LastResourceAdmission = InError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelTerrainResourceAdmission::CapacityBlocked :
		InError.StartsWith(TEXT("Oversized")) || InError.StartsWith(TEXT("CapacityUnachievable")) ?
		EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::InvalidRequest;
	bTickWaitingCapacity = LastResourceAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked;
	TickCapacityVersion = InBeforeAdmission;
	if (!bTickWaitingCapacity)
	{
		UE_LOG(LogTemp, Error, TEXT("Voxel emerge admission failed: %s"), *InError);
	}
	return false;
}

bool FVoxelEmergeManager::ReserveAdmissionAges(const int32 InCount, FString& OutError, uint64& OutBeforeAdmission)
{
	FInputBudget Budget(Scheduler.GetTerrainResources(), GetAdmissionConsumer(), AdmissionAgeStorageLease,
		LastResourceAdmission, OutBeforeAdmission);
	if (!VoxelGenerationAllocation::Private::ReserveSet<TPair<FIntVector, FAdmissionServiceAge>>(
		&Budget, AdmissionAges, InCount, OutError))
	{
		return false;
	}
	if (AdmissionAgeStorageLease)
	{
		AdmissionAgeStorageLease->MarkPublished();
	}
	return true;
}

void FVoxelEmergeManager::ClearDemandRebuild()
{
	DemandRebuild.Demand.Empty();
	DemandRebuild.Order.Empty();
	for (auto& Lane : DemandRebuild.Lanes)
	{
		Lane.Empty();
	}
	DemandRebuild.Critical.Empty();
	DemandRebuild.Ready.Values.Empty();
	DemandRebuild.Ready.Empty();
	DemandRebuild.Queued.Empty();
	DemandRebuild.Lease.Reset();
	DemandRebuild.bActive = false;
	DemandRebuild.Phase = 0;
	DemandRebuild.Cursor = 0;
}

struct FVoxelExactBaseGenerationWork
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BaseLease;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> FactResources;
	FVoxelGenerationConsumerKey Consumer;
	FIntVector Section = FIntVector::ZeroValue;
	TAtomic<bool> bWaitingCapacity{false};
	TAtomic<uint64> CapacityVersion{0};

	EVoxelTaskBlockReason GetBlockReason() const
	{
		if (!bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
		uint64 Version = 0;
		return FactResources->TryGetReleaseVersionExcludingOwner(Consumer, Version) && Version > CapacityVersion.Load()
			? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		if (InCancel.Load())
		{
			Result.bCanceled = true;
			return Result;
		}
		const uint64 BeforeBuild = FactResources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredSourceScope RetentionScope(FactResources.Get(),
			Retention ? &Retention->GetResourceLease().Get() : nullptr);
		bWaitingCapacity.Store(false);
		Result.OutputResourceLease = BaseLease;
		Result.BaseBlocks.Reserve(4096);
		Result.BaseBlocks.SetNumUninitialized(4096);
		Result.bSuccess = Generator->GenerateSection(Section, MakeArrayView(Result.BaseBlocks),
			Result.Error, &InCancel, BaseLease.Get());
		Result.bCanceled = InCancel.Load();
		if (!Result.bSuccess && !Result.bCanceled && Result.Error.StartsWith(TEXT("CapacityBlocked")))
		{
			CapacityVersion.Store(BeforeBuild);
			bWaitingCapacity.Store(true);
			Result.bSuccess = Result.bContinue = true;
			Result.Error.Reset();
		}
		return Result;
	}
};

struct FVoxelExactOverlayDecodeWork
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReadLease;
	FVoxelRegionReadView Read;
	TArray<uint8> Bytes;
	FVoxelDeltaDecoder Decoder;
	FVoxelWorldManifest Manifest;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TAtomic<bool> bWaitingCapacity{false};
	TAtomic<uint64> CapacityVersion{0};

	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		if (InCancel.Load())
		{
			Result.bCanceled = true;
			return Result;
		}
		const uint64 BeforeCapacity = Resources->ReleaseVersionExcludingOwner(Consumer);
		const auto WaitCapacity = [&]
		{
			CapacityVersion.Store(BeforeCapacity);
			bWaitingCapacity.Store(true);
			Result.bSuccess = Result.bContinue = true;
			Result.Error.Reset();
		};
		bWaitingCapacity.Store(false);
		if (Bytes.IsEmpty())
		{
			uint64 RequiredInput = MetadataLease ? MetadataLease->Bytes() : 0;
			int32 RequiredCount = MetadataLease ? 1 : 0;
			const auto SourceLease = Read.GenerationLease ? Read.GenerationLease->GetResourceLease() : nullptr;
			if (SourceLease)
			{
				if (!SourceLease->IsOwnedBy(*Resources))
				{ Result.Error = TEXT("InvalidRequest: Exact Overlay source domain"); return Result; }
				++RequiredCount;
				if (SourceLease->GetPool() == EVoxelTerrainResourcePool::Input)
				{
					if (SourceLease->Bytes() > MAX_uint64 - RequiredInput)
					{ Result.Error = TEXT("CapacityUnachievable: Exact Overlay source floor"); return Result; }
					RequiredInput += SourceLease->Bytes();
				}
			}
			const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working,
				FVoxelRegionStore::ReadWorkingBytes(Read), ReadLease, 0, RequiredInput, RequiredCount);
			if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				WaitCapacity();
				return Result;
			}
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{ Result.Error = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				TEXT("CapacityUnachievable: Exact Overlay frozen read") : TEXT("InvalidRequest: Exact Overlay frozen read"); return Result; }
			const auto Status = FVoxelRegionStore::Read(Read, Bytes, Result.Error);
			if (Status != EVoxelRegionRead::Loaded)
			{
				if (Status == EVoxelRegionRead::Missing)
					Result.Error = TEXT("Modified voxel section is missing from its region file");
				return Result;
			}
		}
		const auto SourceLease = Read.GenerationLease ? Read.GenerationLease->GetResourceLease() : nullptr;
		FVoxelGenerationRequiredSourceScope MetadataScope(Resources.Get(), MetadataLease.Get());
		FVoxelGenerationRequiredSourceScope ReadScope(Resources.Get(), ReadLease.Get());
		FVoxelGenerationRequiredSourceScope GenerationScope(Resources.Get(), SourceLease.Get());
		if (!Decoder.Advance(Bytes, Manifest, *Registry, Resources.ToSharedRef(), Consumer,
			EVoxelTerrainResourcePool::Staged, Result.PersistentSection, Result.Error))
		{
			if (Result.Error.StartsWith(TEXT("CapacityBlocked:"))) WaitCapacity();
			return Result;
		}
		Result.bSuccess = Result.PersistentSection->Section == Read.Section;
		if (!Result.bSuccess) Result.Error = TEXT("Voxel region section identity mismatch");
		return Result;
	}
};

struct FVoxelEnvironmentColumnWork
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> FactResources;
	FVoxelGenerationConsumerKey Consumer;
	FIntPoint Cell = FIntPoint::ZeroValue;
	TAtomic<bool> bWaitingCapacity{false};
	TAtomic<uint64> CapacityVersion{0};

	EVoxelTaskBlockReason GetBlockReason() const
	{
		if (!bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
		uint64 Version = 0;
		return FactResources->TryGetReleaseVersionExcludingOwner(Consumer, Version) && Version > CapacityVersion.Load()
			? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		const uint64 BeforeBuild = FactResources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredSourceScope RetentionScope(FactResources.Get(),
			Retention ? &Retention->GetResourceLease().Get() : nullptr);
		bWaitingCapacity.Store(false);
		FVoxelEnvironmentSample Sample;
		Result.bSuccess = !InCancel.Load() && Generator->SampleEnvironment(Cell.X, Cell.Y, Sample, Result.Error, &InCancel);
		Result.bCanceled = InCancel.Load();
		if (!Result.bSuccess && !Result.bCanceled && Result.Error.StartsWith(TEXT("CapacityBlocked")))
		{
			CapacityVersion.Store(BeforeBuild);
			bWaitingCapacity.Store(true);
			Result.bSuccess = Result.bContinue = true;
			Result.Error.Reset();
		}
		return Result;
	}
};

void FVoxelEmergeManager::VisitStorageSources(const FVoxelTerrainResources& InResources,
	FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	const auto Visit = [&](const FVoxelTerrainResourceLease* Lease)
	{
		if (Lease && Lease->IsOwnedBy(InResources))
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
		}
	};
	const auto VisitRequest = [&](const TOptional<FVoxelTaskRequest>& Request)
	{
		if (Request)
		{
			Visit(Request->ExecuteBindingLease.Get());
			Visit(Request->ApplyBindingLease.Get());
			Visit(Request->BlockBindingLease.Get());
		}
	};
	Visit(FactStorageLease.Get());
	Visit(ExternalDataStorageLease.Get());
	Visit(RemoteChangeStorageLease.Get());
	Visit(DemandStorageLease.Get());
	Visit(DemandRebuild.Lease.Get());
	Visit(AdmissionAgeStorageLease.Get());
	Visit(EnvironmentStorageLease.Get());
	Visit(AdmissionSelection.GetResourceLease());
	for (const auto& Pair : WaitingPlanSections)
	{
		Visit(Pair.Value.Dependencies.Lease.Get());
		if (Pair.Value.Retention)
		{
			Visit(&Pair.Value.Retention->GetResourceLease().Get());
		}
		if (Pair.Value.BaseWork)
		{
			Visit(Pair.Value.BaseWork->MetadataLease.Get());
			Visit(Pair.Value.BaseWork->BaseLease.Get());
			if (Pair.Value.BaseWork->Retention)
			{
				Visit(&Pair.Value.BaseWork->Retention->GetResourceLease().Get());
			}
		}
		if (Pair.Value.OverlayWork)
		{
			Visit(Pair.Value.OverlayWork->MetadataLease.Get());
			Visit(Pair.Value.OverlayWork->ReadLease.Get());
			if (Pair.Value.OverlayWork->Read.GenerationLease)
			{
				Visit(Pair.Value.OverlayWork->Read.GenerationLease->GetResourceLease().Get());
			}
		}
		VisitRequest(Pair.Value.PendingRequest);
	}
	for (const auto& Pair : ExternalFactConsumers)
	{
		Visit(Pair.Value.Dependencies.Lease.Get());
		Visit(Pair.Value.SampleRetentionStorageLease.Get());
		for (const auto& Retention : Pair.Value.SampleRetentions)
		{
			Visit(&Retention->GetResourceLease().Get());
		}
		if (Pair.Value.Retention)
		{
			Visit(&Pair.Value.Retention->GetResourceLease().Get());
		}
	}
	for (const auto& Pair : EnvironmentColumnStates)
	{
		if (Pair.Value.Work)
		{
			Visit(Pair.Value.Work->MetadataLease.Get());
			if (Pair.Value.Work->Retention)
			{
				Visit(&Pair.Value.Work->Retention->GetResourceLease().Get());
			}
		}
		VisitRequest(Pair.Value.PendingRequest);
	}
}

FVoxelEmergeManager::FVoxelEmergeManager(
	FVoxelWorldRuntime& InRuntime,
	FVoxelTaskScheduler& InScheduler,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	const FVoxelRegionStore& InRegionStore,
	const FVoxelWorldManifest& InManifest,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry)
	: Runtime(InRuntime)
	, Scheduler(InScheduler)
	, Generator(InGenerator)
	, PlanCoordinator(MakeUnique<FVoxelGenerationPlanCoordinator>(InScheduler,
		InConfig, InCache, InRuntime.Epoch()))
	, RegionStore(InRegionStore)
	, Manifest(InManifest)
	, Registry(InRegistry)
{
	AdmissionSelection.BindResources(Scheduler.GetTerrainResources(),
		FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), GenerationOwnerId, 1, {}, 1));
	Generator->BindRetentionResources(Scheduler.GetRuntimeResources(),
		{EVoxelGenerationConsumerKind::WorldFact, Runtime.Epoch(), GenerationOwnerId, 1,
			EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1});
	FVoxelGenerationPlanningServices PlanningServices;
	PlanningServices.Resources = Scheduler.GetRuntimeResources();
	PlanningServices.TaskResources = Scheduler.GetTerrainResources();
	PlanningServices.TaskControlConsumer = Scheduler.GetControlConsumer();
	PlanningServices.RetentionConsumer = {EVoxelGenerationConsumerKind::WorldFact, Runtime.Epoch(), GenerationOwnerId, 1,
		EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1};
	PlanningServices.Ensure = [this](TConstArrayView<FVoxelGenerationPlanDependency> Dependencies, EVoxelWorkClass WorkClass,
		const FVoxelTerrainServicePriority& Priority, const FVoxelGenerationConsumerKey& Consumer)
	{
		return PlanCoordinator->Ensure(Dependencies, WorkClass, Priority, Consumer);
	};
	PlanningServices.Release = [this](const FVoxelGenerationConsumerKey& Consumer) { PlanCoordinator->ReleaseConsumer(Consumer); };
	PlanningServices.Cancel = [this](const FVoxelTaskStamp& Expected)
	{
		Scheduler.CancelMatching([&Expected](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::ProjectBackground && Stamp == Expected;
		});
	};
	PlanningServices.Retain = [this](TConstArrayView<FVoxelGenerationPlanDependency> Dependencies, const FIntPoint& Center,
		const FVoxelGenerationConsumerKey& Consumer, TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutRetention,
		FString& OutError)
	{
		return PlanCoordinator->RetainDependencies(Dependencies, Center, Consumer, OutRetention, OutError);
	};
	Generator->SetOverlayPlanningServices(MoveTemp(PlanningServices));
	PlanCoordinator->SetConsumerWakeCallback(
		[this](const FVoxelGenerationConsumerKey& Consumer)
		{
			OnGenerationFactsReady(Consumer);
		});
	Generator->SetTypedOverlayConsumerWakeCallback(
		[this](const FVoxelGenerationConsumerKey& Consumer)
		{
			OnGenerationFactsReady(Consumer);
		});
	Generator->SetOverlayServicePriorityDonation(
		[this](const FVoxelTaskStamp& Stamp, const EVoxelWorkClass InClass, const FVoxelTerrainServicePriority& InPriority)
		{
			if (Stamp.WorldEpoch != Runtime.Epoch() || !Stamp.OwnerId.IsValid() || Stamp.OwnerIncarnation == 0) return;
			Scheduler.UpdateServicePriorities([&](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Current,
				EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
			{
				if (Kind == EVoxelTaskKind::ProjectBackground && Current == Stamp)
				{
					WorkClass = InClass;
					Priority = InPriority;
				}
			});
		});
	Generator->SetOverlayConsumerWakeCallback(
		[this](const FIntVector& Key)
		{
			WakeCriticalSection(Key);
		});
}

void FVoxelEmergeManager::Tick(const FVoxelInterestSet& InInterest, const uint64 InInterestRevision, const double InNow,
	const int32 InMaxBuildsPerFrame, const double InAdmissionMilliseconds,
	const FVoxelTerrainFrameContext* InFrameContext)
{
	if (bResetting)
	{
		return;
	}
	const double Started = FPlatformTime::Seconds();
	const double Deadline = InFrameContext ? InFrameContext->Deadline(InAdmissionMilliseconds) :
		Started + FMath::Max(0.0, InAdmissionMilliseconds) / 1000.0;
	if (InNow - LastAdmissionDiagnosticTime >= 5.0)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Voxel emerge admission: desired=%llu applied=%llu priority=%llu/%llu rebuild=%d phase=%d cursor=%d waitingCapacity=%d admission=%d budgetMs=%.3f exact=%d critical=%d ready=%d blocked=%d fallback=%d planPending=%d overlayPending=%d enqueued=%d"),
			InInterestRevision, CurrentInterestRevision, InInterest.PriorityRevision, CurrentPriorityRevision,
			DemandRebuild.bActive, DemandRebuild.Phase, DemandRebuild.Cursor, bTickWaitingCapacity,
			static_cast<int32>(LastResourceAdmission), (Deadline - Started) * 1000.0, CurrentDemand.Num(), CriticalAdmissionKeys.Num(), QueuedCriticalSections.Num(), WaitingPlanSections.Num(), CriticalFallbackCursor,
			PlanPendingAttempts, OverlayPendingAttempts, EnqueuedTasks);
		int32 Reported = 0;
		for (const auto& Pair : WaitingPlanSections)
		{
			if (Reported >= 8 || FPlatformTime::Seconds() >= Deadline)
			{
				break;
			}
			const FVoxelExactDemand* Demand = CurrentDemand.Find(Pair.Key);
			if (!Demand || !IsCriticalDemand(*Demand))
			{
				continue;
			}
			const FVoxelSection* Section = Runtime.FindSection(Pair.Key);
			UE_LOG(LogTemp, Display,
				TEXT("Voxel critical blocked: section=(%d,%d,%d) epoch=%llu token=%llu serial=%llu status=%d ageMs=%.2f planReady=%d overlayReady=%d factCapacity=%d taskCapacity=%d"),
				Pair.Key.X, Pair.Key.Y, Pair.Key.Z, Pair.Value.Stamp.Epoch, Pair.Value.Stamp.Token, Pair.Value.Consumer.RequestSerial,
				Section ? static_cast<int32>(Section->Status) : -1, (InNow - Pair.Value.BlockedSinceSeconds) * 1000.0,
				Pair.Value.bPlanReady, Pair.Value.bOverlayReady, Pair.Value.bWaitingCapacity, Pair.Value.bWaitingTaskCapacity);
			++Reported;
		}
		LastAdmissionDiagnosticTime = InNow;
		PlanPendingAttempts = 0;
		OverlayPendingAttempts = 0;
		EnqueuedTasks = 0;
	}
	if (Started >= Deadline)
	{
		return;
	}
	Generator->AdvanceOverlayConsumerWakes(Deadline, 64);
	PlanCoordinator->AdvanceConsumerWakes(Deadline, 64);
	if (FPlatformTime::Seconds() >= Deadline)
	{
		return;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = GetAdmissionConsumer();
	const bool bChangedRebuild = DemandRebuild.bActive && DemandRebuild.Phase < 7 && (DemandRebuild.InterestRevision != InInterestRevision ||
		DemandRebuild.PriorityRevision != InInterest.PriorityRevision);
	if (bChangedRebuild)
	{
		ClearDemandRebuild();
		bTickWaitingCapacity = false;
	}
	if (LastResourceAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
		LastResourceAdmission == EVoxelTerrainResourceAdmission::InvalidRequest)
	{
		return;
	}
	if (bTickWaitingCapacity && Resources->ReleaseVersionExcludingOwner(Consumer) <= TickCapacityVersion)
	{
		return;
	}
	bTickWaitingCapacity = false;
	struct FAdmissionSources
	{
		const FVoxelEmergeManager* Manager;
		const FVoxelInterestSet* Interest;
		const FVoxelTerrainResources* Resources;
	};
	const FAdmissionSources Sources{this, &InInterest, &Resources.Get()};
	FVoxelGenerationRequiredSourceScope SourceScope(&Resources.Get(), &Sources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Source = *static_cast<const FAdmissionSources*>(InContext);
			const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
			{
				if (Lease && Lease->IsOwnedBy(*Source.Resources))
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
				}
			};
			Visit(Source.Manager->DemandStorageLease);
			Visit(Source.Manager->DemandRebuild.Lease);
			Visit(Source.Manager->AdmissionAgeStorageLease);
			Visit(Source.Manager->EnvironmentStorageLease);
			Visit(Source.Manager->ExternalDataStorageLease);
			Visit(Source.Manager->RemoteChangeStorageLease);
			const auto* SelectionLease = Source.Manager->AdmissionSelection.GetResourceLease();
			if (SelectionLease && SelectionLease->IsOwnedBy(*Source.Resources))
			{
				FVoxelGenerationRequiredSourceScope::VisitLease(SelectionLease, InVisit);
			}
			Visit(Source.Interest->DemandLease);
			Visit(Source.Interest->CompositionLease);
			Visit(Source.Interest->CopyLease);
			Visit(Source.Interest->FinalizationLease);
			Visit(Source.Interest->CoarseCompositionLease);
			for (const auto& Lease : Source.Interest->CoarsePlanLeases)
			{
				Visit(Lease);
			}
		});
	if (CurrentInterestRevision != InInterestRevision || CurrentPriorityRevision != InInterest.PriorityRevision || DemandRebuild.bActive)
	{
		if (!RebuildDemand(InInterest, InInterestRevision, Deadline) || CurrentInterestRevision != InInterestRevision ||
			CurrentPriorityRevision != InInterest.PriorityRevision)
		{
			return;
		}
	}
	if (FPlatformTime::Seconds() >= Deadline)
	{
		return;
	}
	if (CurrentDemand.IsEmpty() && ExternalDataDemands.IsEmpty())
	{
		LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
		return;
	}
	FString Error;
	uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!AdmissionSelection.Prepare(Error))
	{
		DeferTickAdmission(BeforeAdmission, Error);
		return;
	}
	const int32 BuildLimit = FMath::Clamp(InMaxBuildsPerFrame, 1, 1024);
	int32 Submitted = 0;
	constexpr int32 ReadySteps = 16;
	constexpr int32 ExternalSteps = 32;
	constexpr int32 LaneSteps = 32;
	constexpr int32 CandidateLimit = ReadySteps + ExternalSteps + 2 * LaneSteps;
	FVoxelGenerationWorkspace Workspace(Resources, Consumer);
	TArray<FVoxelTerrainServiceCandidate> Candidates;
	TArray<FIntVector> CandidateKeys;
	TSet<FIntVector> Captured;
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Workspace.ReserveArray(Candidates, CandidateLimit, Error) ||
		!Workspace.ReserveArray(CandidateKeys, CandidateLimit, Error) ||
		!Workspace.ReserveSet(Captured, CandidateLimit, Error))
	{
		DeferTickAdmission(BeforeAdmission, Error);
		return;
	}
	const auto CaptureCandidate = [this, &Candidates, &CandidateKeys, &Captured, &Error, &BeforeAdmission, InNow](const FIntVector& Key)
	{
		if (Captured.Contains(Key))
		{
			return true;
		}
		Captured.Add(Key);
		const FVoxelExactDemand* Demand = FindDemand(Key);
		if (!Demand)
		{
			return true;
		}
		FVoxelSection* Section = Runtime.FindSection(Key);
		if (Section && (Section->Status == EVoxelSectionStatus::DataReady || Section->Status == EVoxelSectionStatus::Failed))
		{
			if (Section->Status == EVoxelSectionStatus::DataReady)
			{
				Section->bSimulationWanted = Demand->bSimulation;
			}
			AdmissionAges.Remove(Key);
			return true;
		}
		const auto* Waiting = WaitingPlanSections.Find(Key);
		if (Waiting && InNow < Waiting->RetryAfterSeconds)
		{
			return true;
		}
		const uint64 Token = Section ? Section->Stamp.Token : 0;
		FAdmissionServiceAge* Age = AdmissionAges.Find(Key);
		const bool bNewAge = !Age || (Age->SectionToken != 0 && Token != Age->SectionToken);
		if (bNewAge)
		{
			if (NextAdmissionSequence == MAX_uint64 || (!Age && AdmissionAges.Num() == MAX_int32))
			{
				Error = TEXT("Oversized: emerge admission service identity");
				return false;
			}
			if (!Age && !ReserveAdmissionAges(AdmissionAges.Num() + 1, Error, BeforeAdmission))
			{
				return false;
			}
			Age = &AdmissionAges.Add(Key, {FPlatformTime::Seconds(), ++NextAdmissionSequence, Token});
		}
		else if (Token != 0)
		{
			Age->SectionToken = Token;
		}
		FVoxelTerrainServiceCandidate Candidate;
		Candidate.Index = Candidates.Num();
		Candidate.Priority = Demand->ServicePriority;
		Candidate.FirstQueuedSeconds = Age->FirstQueuedSeconds;
		Candidate.Sequence = Age->Sequence;
		Candidate.EstimatedMicroseconds = AdmissionCostEstimateMicroseconds;
		Candidate.bRunnable = true;
		Candidates.Add(Candidate);
		CandidateKeys.Add(Key);
		return true;
	};
	FIntVector Woken;
	for (int32 Attempt = 0; Attempt < ReadySteps && FPlatformTime::Seconds() < Deadline && ReadyCriticalSections.Peek(Woken); ++Attempt)
	{
		if (!CaptureCandidate(Woken))
		{
			DeferTickAdmission(BeforeAdmission, Error);
			return;
		}
		const bool bDequeued = ReadyCriticalSections.Dequeue(Woken);
		check(bDequeued);
		QueuedCriticalSections.Remove(Woken);
	}
	// 独立数据消费者不修改已提交的 Interest；局部稀疏游标仍进入同一服务选择器。
	for (int32 Step = 0; Step < ExternalSteps && !ExternalDataDemands.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const int32 Index = ExternalDataCursor % ExternalDataOrder.GetMaxIndex();
		if (ExternalDataOrder.IsValidIndex(Index) && !CaptureCandidate(ExternalDataOrder[Index]))
		{
			DeferTickAdmission(BeforeAdmission, Error);
			return;
		}
		ExternalDataCursor = Index + 1;
	}
	for (int32 LaneIndex = 0; LaneIndex < 2 && FPlatformTime::Seconds() < Deadline; ++LaneIndex)
	{
		const auto& Lane = AdmissionLanes[LaneIndex];
		int32& Scan = NextAdmissionIndices[LaneIndex];
		for (int32 Attempt = 0; Attempt < FMath::Min(LaneSteps, Lane.Num()) && FPlatformTime::Seconds() < Deadline; ++Attempt)
		{
			const int32 Index = Scan % Lane.Num();
			if (!CaptureCandidate(OrderedKeys[Lane[Index]]))
			{
				DeferTickAdmission(BeforeAdmission, Error);
				return;
			}
			Scan = Index + 1;
		}
	}
	for (int32 Attempt = 0; Attempt < Candidates.Num() && Submitted < BuildLimit && FPlatformTime::Seconds() < Deadline; ++Attempt)
	{
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = InFrameContext && InFrameContext->IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Selected = AdmissionSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Selected == INDEX_NONE)
		{
			break;
		}
		if (Charge.bOverdueOpportunity)
		{
			InFrameContext->ConsumeOverdueOpportunity();
		}
		Candidates[Selected].bRunnable = false;
		const FIntVector Key = CandidateKeys[Selected];
		const double ServiceStarted = FPlatformTime::Seconds();
		const auto* Demand = FindDemand(Key);
		const bool bSubmitted = Demand && RequestSection(Key, *Demand);
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - ServiceStarted) * 1000000.0);
		const bool bSettled = AdmissionSelection.Settle(Charge.Serial, Actual);
		check(bSettled);
		AdmissionCostEstimateMicroseconds = FMath::Lerp(AdmissionCostEstimateMicroseconds, Actual, 0.15);
		Submitted += bSubmitted ? 1 : 0;
		if (bSubmitted)
		{
			AdmissionAges.Remove(Key);
		}
	}
	LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;

}

bool FVoxelEmergeManager::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::GenerateExactBase && InResult.Kind != EVoxelTaskKind::DecodeOverlay) return false;
	FVoxelSection* Section = Runtime.FindSection(InResult.Stamp.Section);
	if (!Section) return true;
	const FVoxelSectionStamp ExpectedStamp{InResult.Stamp.WorldEpoch, InResult.Stamp.Token, Section->Stamp.RecipeHash};
	const FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InResult.Stamp.Section);
	if (!Runtime.IsCurrentStamp(InResult.Stamp.Section, ExpectedStamp) || InResult.Stamp.OwnerId != GenerationOwnerId ||
		InResult.Stamp.OwnerIncarnation != Section->Stamp.Token || !Waiting || Waiting->Stamp != ExpectedStamp ||
		Waiting->Consumer.RequestSerial != InResult.Stamp.Revision) return true;
	const FVoxelGenerationConsumerKey Consumer = Waiting->Consumer;
	const FIntVector Key = InResult.Stamp.Section;
	const auto FinishAttempt = [this, Key, ExpectedStamp, Consumer]
	{
		if (const FWaitingPlanSection* Current = WaitingPlanSections.Find(Key);
			Current && Current->Stamp == ExpectedStamp && Current->Consumer == Consumer) WaitingPlanSections.Remove(Key);
		PlanCoordinator->ReleaseConsumer(Consumer);
		Generator->ReleaseOverlayConsumer(Consumer);
	};
	if (InResult.bCanceled)
	{
		FinishAttempt();
		WakeCriticalSection(Key);
		return true;
	}
	if (!InResult.bSuccess)
	{
		if (InResult.Kind == EVoxelTaskKind::GenerateExactBase && InResult.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			Section->Status = EVoxelSectionStatus::Allocated;
			Section->Error.Reset();
			FinishAttempt();
			WakeCriticalSection(Key);
			return true;
		}
		UE_LOG(LogTemp, Error, TEXT("Voxel section generation failed: section=%s kind=%d error=%s"),
			*Key.ToString(), static_cast<int32>(InResult.Kind), *InResult.Error);
		Section->Status = EVoxelSectionStatus::Failed;
		Section->Error = InResult.Error;
		FinishAttempt();
		return true;
	}
	FString Error;
	const uint64 BeforePublication = Runtime.GetTerrainResources()->ReleaseVersionExcludingOwner(Consumer);
	if (InResult.Kind == EVoxelTaskKind::DecodeOverlay)
	{
		if (!InResult.PersistentSection)
		{
			Section->Status = EVoxelSectionStatus::Failed;
			Section->Error = TEXT("InvalidRequest: exact Overlay result is missing");
			FinishAttempt();
			return true;
		}
		if (!Runtime.PublishFinal(Key, InResult.PersistentSection->Revision, InResult.PersistentSection->Blocks,
			InResult.PersistentSection->Entities, Error))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked:")))
			{
				Section->PendingCapacityVersion = BeforePublication;
				*Section->PendingPublish = MoveTemp(InResult);
				return true;
			}
			Section->Status = EVoxelSectionStatus::Failed;
			Section->Error = Error;
		}
		else InResult.MarkWorkUsed();
		FinishAttempt();
		return true;
	}
	if (!Runtime.PublishBase(Key, ExpectedStamp, MoveTemp(InResult.BaseBlocks), Error, InResult.OutputResourceLease))
	{
		if (Error.StartsWith(TEXT("CapacityBlocked:")))
		{
			Section->PendingCapacityVersion = BeforePublication;
			*Section->PendingPublish = MoveTemp(InResult);
			return true;
		}
		Section->Status = EVoxelSectionStatus::Failed;
		Section->Error = Error;
		FinishAttempt();
		return true;
	}
	InResult.MarkWorkUsed();
	FinishAttempt();
	Section = Runtime.FindSection(Key);
	if (Section && Runtime.IsCurrentStamp(Key, ExpectedStamp))
	{
		if (const FVoxelExactDemand* Demand = FindDemand(Key)) ResolveOverlay(*Section, Key, *Demand);
	}
	return true;
}

bool FVoxelEmergeManager::SetRemoteChangeState(const FIntVector& InSection,
	const EVoxelSectionChangeState InState, FString& OutError)
{
	check(IsInGameThread());
	const auto Resources = Scheduler.GetTerrainResources();
	if (bResetting || (InState != EVoxelSectionChangeState::Unknown && InState != EVoxelSectionChangeState::Natural &&
		InState != EVoxelSectionChangeState::Modified))
	{
		RemoteChangeAdmission.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		OutError = TEXT("InvalidRequest: remote section change state");
		return false;
	}
	if (InState == EVoxelSectionChangeState::Unknown)
	{
		RemoteChangeStates.Remove(InSection);
	}
	else
	{
		if (!RemoteChangeStates.Contains(InSection))
		{
			if (RemoteChangeStates.Num() == MAX_int32)
			{
				RemoteChangeAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
				OutError = TEXT("Oversized: remote section change directory");
				return false;
			}
			const int32 Count = RemoteChangeStates.Num() + 1;
			const bool bCanReserve = VoxelGenerationAllocation::Private::SetCanReserve<TPair<FIntVector, EVoxelSectionChangeState>>(
				RemoteChangeStates, Count);
			if (!bCanReserve && !CanRetryStorage(RemoteChangeAdmission, *Resources,
				TEXT("CapacityBlocked: remote section change directory awaiting external release"), OutError))
			{
				RemoteChangeAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityBlocked;
				return false;
			}
			FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
				[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
				{
					const auto& Manager = *static_cast<const FVoxelEmergeManager*>(InContext);
					Manager.VisitStorageSources(*Manager.Scheduler.GetTerrainResources(), InVisit);
				});
			FInputBudget Budget(Resources, GetAdmissionConsumer(), RemoteChangeStorageLease,
				RemoteChangeAdmission.Admission, RemoteChangeAdmission.CapacityVersion);
			if (!VoxelGenerationAllocation::Private::ReserveSet<TPair<FIntVector, EVoxelSectionChangeState>>(
				&Budget, RemoteChangeStates, Count, OutError))
			{
				RemoteChangeAdmission.bWaitingCapacity = RemoteChangeAdmission.Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
				return false;
			}
			if (!bCanReserve)
			{
				RemoteChangeAdmission.bWaitingCapacity = false;
			}
		}
		RemoteChangeStates.Add(InSection, InState);
		if (RemoteChangeStorageLease)
		{
			RemoteChangeStorageLease->MarkPublished();
		}
	}
	if (RemoteChangeStates.IsEmpty())
	{
		RemoteChangeStates.Empty();
		RemoteChangeStorageLease.Reset();
		RemoteChangeAdmission.bWaitingCapacity = false;
	}
	RemoteChangeAdmission.Admission = EVoxelTerrainResourceAdmission::Granted;
	OutError.Reset();
	FVoxelSection* Section = Runtime.FindSection(InSection);
	const FVoxelExactDemand* Demand = FindDemand(InSection);
	if (Section && Demand && Section->Status == EVoxelSectionStatus::BaseReady)
	{
		ResolveOverlay(*Section, InSection, *Demand);
	}
	return true;
}

FVoxelEmergeManager::EEnvironmentFailure FVoxelEmergeManager::ClassifyEnvironmentFailure(const FString& InError)
{
	if (InError.StartsWith(TEXT("Oversized")))
	{
		return EEnvironmentFailure::Oversized;
	}
	if (InError.StartsWith(TEXT("CapacityUnachievable")))
	{
		return EEnvironmentFailure::CapacityUnachievable;
	}
	if (InError.StartsWith(TEXT("InvalidRequest")) || InError.StartsWith(TEXT("InvalidAllocationScope")) ||
		InError.StartsWith(TEXT("AllocationAccountingMismatch")))
	{
		return EEnvironmentFailure::InvalidRequest;
	}
	if (InError.StartsWith(TEXT("InvalidResource")))
	{
		return EEnvironmentFailure::InvalidResource;
	}
	if (InError.StartsWith(TEXT("InvalidConsumer")))
	{
		return EEnvironmentFailure::InvalidConsumer;
	}
	return EEnvironmentFailure::Failure;
}

const TCHAR* FVoxelEmergeManager::GetEnvironmentFailureError(const EEnvironmentFailure InFailure)
{
	switch (InFailure)
	{
	case EEnvironmentFailure::Oversized: return TEXT("Oversized: environment column request failed");
	case EEnvironmentFailure::CapacityUnachievable: return TEXT("CapacityUnachievable: environment column request failed");
	case EEnvironmentFailure::InvalidRequest: return TEXT("InvalidRequest: environment column request failed");
	case EEnvironmentFailure::InvalidResource: return TEXT("InvalidResource: environment column request failed");
	case EEnvironmentFailure::InvalidConsumer: return TEXT("InvalidConsumer: environment column request failed");
	default: return TEXT("Environment column generation failed");
	}
}

bool FVoxelEmergeManager::ReserveEnvironmentStorage(FString& OutError)
{
	if (EnvironmentStorageLease) return true;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey{EVoxelGenerationConsumerKind::Environment, Runtime.Epoch(),
		EnvironmentTaskOwnerId, EnvironmentTaskIncarnation, EVoxelGenerationSpatialKind::Column,
		FIntVector::ZeroValue, FIntVector::ZeroValue, 1};
	const uint64 Bytes = VoxelBoundedAllocation::MapBytes<FIntPoint, FEnvironmentColumnAttempt>(32);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: environment request storage") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: environment request storage") :
			TEXT("InvalidRequest: environment request storage");
		return false;
	}
	EnvironmentColumnStates.Reserve(32);
	const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(EnvironmentColumnStates);
	check(Actual <= Bytes);
	const bool bSettled = Lease->Resize(Actual);
	check(bSettled);
	Lease->MarkPublished();
	EnvironmentStorageLease = MoveTemp(Lease);
	OutError.Reset();
	return true;
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::DeferEnvironmentAdmission(const FIntPoint& InCell,
	FEnvironmentColumnAttempt& InAttempt, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const uint64 InBeforeAdmission, const FString& InError)
{
	const bool bTemporary = InError.StartsWith(TEXT("CapacityBlocked"));
	Scheduler.ObserveDemand(EVoxelTaskKind::ProjectBackground, InAttempt.Stamp,
		bTemporary ? EVoxelTaskBlockReason::InputMemory : InError.StartsWith(TEXT("CapacityUnachievable")) ||
			InError.StartsWith(TEXT("Oversized")) ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	if (bTemporary)
	{
		InAttempt.AdmissionResources = InResources;
		InAttempt.AdmissionCapacityVersion = InBeforeAdmission;
		InAttempt.bWaitingCapacity = true;
		return {false, false, InError};
	}
	InAttempt.PendingRequest.Reset();
	InAttempt.Work.Reset();
	InAttempt.AdmissionResources.Reset();
	InAttempt.Failure = ClassifyEnvironmentFailure(InError);
	const EEnvironmentFailure Failure = InAttempt.Failure;
	UE_LOG(LogTemp, Error, TEXT("Environment column (%d,%d) request serial=%llu failed: %s"),
		InCell.X, InCell.Y, InAttempt.Consumer.RequestSerial, *InError);
	PlanCoordinator->ReleaseEnvironment(InCell);
	return {false, true, GetEnvironmentFailureError(Failure)};
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample)
{
	check(IsInGameThread());
	if (bResetting) return {};
	if (InCell.X == MAX_int32 || InCell.Y == MAX_int32)
	{
		return {false, true, TEXT("Oversized: environment query exceeds world coordinates")};
	}
	if (Generator->TrySampleEnvironment(InCell, OutSample))
	{
		PlanCoordinator->ReleaseEnvironment(InCell);
		if (const auto* Current = EnvironmentColumnStates.Find(InCell); Current && !Current->bSubmitted)
		{
			const FVoxelTaskStamp Stamp = Current->Stamp;
			EnvironmentColumnStates.Remove(InCell);
			Scheduler.CancelMatching([Stamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Candidate)
			{
				return Kind == EVoxelTaskKind::ProjectBackground && Candidate == Stamp;
			});
		}
		return {true, false, {}};
	}
	FEnvironmentColumnAttempt* Attempt = EnvironmentColumnStates.Find(InCell);
	if (Attempt && Attempt->Failure != EEnvironmentFailure::None)
	{
		return {false, true, GetEnvironmentFailureError(Attempt->Failure)};
	}
	if (!Attempt)
	{
		if (EnvironmentColumnStates.Num() >= 32) return {};
		FString Error;
		if (!ReserveEnvironmentStorage(Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		if (NextEnvironmentTaskSerial == MAX_uint64) return {false, true, TEXT("Oversized: environment task serial")};
		Attempt = &EnvironmentColumnStates.Add(InCell);
		Attempt->Stamp.OwnerId = EnvironmentTaskOwnerId;
		Attempt->Stamp.OwnerIncarnation = EnvironmentTaskIncarnation;
		Attempt->Stamp.WorldEpoch = Runtime.Epoch();
		Attempt->Stamp.Section = FIntVector(InCell.X, InCell.Y, 0);
		Attempt->Stamp.Token = ++NextEnvironmentTaskSerial;
		Attempt->Stamp.Revision = 1;
		Attempt->Consumer = {EVoxelGenerationConsumerKind::Environment, Attempt->Stamp.WorldEpoch,
			Attempt->Stamp.OwnerId, Attempt->Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Column,
			Attempt->Stamp.Section, Attempt->Stamp.Section, Attempt->Stamp.Token};
		Attempt->PendingRequest.Emplace();
		auto& Request = Attempt->PendingRequest.GetValue();
		Request.Kind = EVoxelTaskKind::ProjectBackground;
		Request.WorkClass = EVoxelWorkClass::Interactive;
		Request.Stamp = Attempt->Stamp;
		Request.GenerationConsumer = Attempt->Consumer;
		Request.bResumable = true;
		// 环境正文在原Fact账本接纳；此完成结果仅使用已授予Slot内联字段。
		Request.ReservedBytes = 1;
	}
	if (Attempt->Stamp.WorldEpoch != Runtime.Epoch() || Attempt->Stamp.OwnerId != EnvironmentTaskOwnerId ||
		Attempt->Stamp.OwnerIncarnation != EnvironmentTaskIncarnation) return {false, true, TEXT("InvalidRequest: environment attempt identity")};
	if (Attempt->bSubmitted) return {};
	if (Attempt->bWaitingCapacity && Attempt->AdmissionResources->ReleaseVersionExcludingOwner(Attempt->Consumer) <=
		Attempt->AdmissionCapacityVersion) return {};
	Attempt->bWaitingCapacity = false;
	const FVoxelTaskStamp Stamp = Attempt->Stamp;
	const FVoxelGenerationConsumerKey Consumer = Attempt->Consumer;
	const FVoxelGenerationDependencyStatus Status = PlanCoordinator->EnsureEnvironment(InCell);
	Attempt = EnvironmentColumnStates.Find(InCell);
	if (!Attempt || Attempt->Stamp != Stamp || Attempt->Consumer != Consumer) return {};
	if (!Status.bReady)
	{
		if (Status.bFailed)
		{
			Attempt->PendingRequest.Reset();
			Attempt->Work.Reset();
			Attempt->Failure = ClassifyEnvironmentFailure(Status.Error);
			UE_LOG(LogTemp, Error, TEXT("Environment column (%d,%d) request serial=%llu failed: %s"),
				InCell.X, InCell.Y, Consumer.RequestSerial, *Status.Error);
			PlanCoordinator->ReleaseEnvironment(InCell);
		}
		return Status;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Attempt->Work)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelEnvironmentColumnWork>(), Lease, 0,
			EnvironmentStorageLease->Bytes(), 1);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			const FString Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: environment worker") :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: environment worker") :
				TEXT("InvalidRequest: environment worker");
			return DeferEnvironmentAdmission(InCell, *Attempt, Resources, BeforeAdmission, Error);
		}
		Attempt->Work = VoxelBoundedAllocation::MakeWorkerShared<FVoxelEnvironmentColumnWork>(Lease);
		Attempt->Work->MetadataLease = MoveTemp(Lease);
		Attempt->Work->Generator = Generator;
		Attempt->Work->FactResources = Scheduler.GetRuntimeResources();
		Attempt->Work->Consumer = Consumer;
		Attempt->Work->Cell = InCell;
	}
	const auto Work = Attempt->Work;
	FString Error;
	if (!Work->Retention)
	{
		const auto FactResources = Scheduler.GetRuntimeResources();
		const uint64 BeforeRetention = FactResources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationConsumerScope ConsumerScope(Consumer);
		Work->Retention = Generator->RetainHydrologyForPlanning(InCell, 0, &Error);
		if (!Work->Retention)
		{
			if (Error.IsEmpty()) Error = TEXT("InvalidRequest: environment retention rejected without reason");
			return DeferEnvironmentAdmission(InCell, *Attempt, FactResources, BeforeRetention, Error);
		}
	}
	auto& Request = Attempt->PendingRequest.GetValue();
	Request.InputBytes = Work->MetadataLease->Bytes();
	Request.SetRequiredBindingInput(EnvironmentStorageLease->Bytes() + Work->MetadataLease->Bytes(), 2);
	if (!Request.bBlockBound)
	{
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if (!Request.BindGetBlockReason(Resources, Consumer, [Work] { return Work->GetBlockReason(); }, Error))
			return DeferEnvironmentAdmission(InCell, *Attempt, Resources, BeforeAdmission, Error);
	}
	if (!Request.bExecuteBound)
	{
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if (!Request.BindExecute(Resources, Consumer,
			[Work](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel); }, Error))
			return DeferEnvironmentAdmission(InCell, *Attempt, Resources, BeforeAdmission, Error);
	}
	if (!Request.bApplyBound)
	{
		BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if (!Request.BindApply(Resources, Consumer, [this, InCell, Consumer](FVoxelTaskResult&& Result)
			{
				FEnvironmentColumnAttempt* Current = EnvironmentColumnStates.Find(InCell);
				if (!Current || Current->Stamp != Result.Stamp || Current->Consumer != Consumer ||
					Result.Stamp.WorldEpoch != Runtime.Epoch() || Result.Stamp.OwnerId != EnvironmentTaskOwnerId ||
					Result.Stamp.OwnerIncarnation != EnvironmentTaskIncarnation) return;
				if (!Result.bSuccess && !Result.bCanceled && !Result.Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Result.Error.StartsWith(TEXT("CapacityBlocked")))
				{
					Current->bSubmitted = false;
					Current->Failure = ClassifyEnvironmentFailure(Result.Error);
					PlanCoordinator->ReleaseEnvironment(InCell);
					UE_LOG(LogTemp, Error, TEXT("Environment column (%d,%d) request serial=%llu failed: %s"),
						InCell.X, InCell.Y, Consumer.RequestSerial, *Result.Error);
				}
				else
				{
					if (Result.bSuccess && !Result.bCanceled) Result.MarkWorkUsed();
					EnvironmentColumnStates.Remove(InCell);
				}
			}, Error))
			return DeferEnvironmentAdmission(InCell, *Attempt, Resources, BeforeAdmission, Error);
	}
	FVoxelTaskRequest Submitted = MoveTemp(Request);
	Attempt->PendingRequest.Reset();
	Attempt->bSubmitted = true;
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const bool bEnqueued = Scheduler.Enqueue(MoveTemp(Submitted));
	Attempt = EnvironmentColumnStates.Find(InCell);
	if (!Attempt || Attempt->Stamp != Stamp || Attempt->Consumer != Consumer) return {};
	if (bEnqueued)
	{
		Attempt->Work.Reset();
		Attempt->AdmissionResources.Reset();
		return {};
	}
	Attempt->bSubmitted = false;
	Attempt->PendingRequest.Emplace(MoveTemp(Submitted));
	const auto Reason = Attempt->PendingRequest->Blocked;
	if (Reason == EVoxelTaskBlockReason::Oversized || Reason == EVoxelTaskBlockReason::TerminalFailure)
	{
		return DeferEnvironmentAdmission(InCell, *Attempt, Resources, BeforeAdmission,
			Reason == EVoxelTaskBlockReason::Oversized ? TEXT("CapacityUnachievable: environment task admission") :
			TEXT("InvalidRequest: environment task admission"));
	}
	if (Reason == EVoxelTaskBlockReason::InputMemory || Reason == EVoxelTaskBlockReason::Memory)
	{
		Attempt->AdmissionResources = Resources;
		Attempt->AdmissionCapacityVersion = BeforeAdmission;
		Attempt->bWaitingCapacity = true;
	}
	return {};
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::EnsureEnvironmentBounds(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority, const double InDistanceScore, const double InForwardScore)
{
	return PlanCoordinator->EnsureEnvironmentBounds(InBounds, InWorkClass,
		InSourcePriority, InDistanceScore, InForwardScore);
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::EnsureEnvironmentSamples(
	const FVoxelGenerationConsumerKey& InConsumer, const FIntPoint& InOrigin, const int32 InWidth,
	const int32 InHeight, const int32 InStep, const double InDistanceScore, bool& bOutNeedsAdvance)
{
	check(IsInGameThread());
	bOutNeedsAdvance = false;
	if (!InConsumer.IsValid() || InConsumer.WorldEpoch != Runtime.Epoch() ||
		InConsumer.Kind != EVoxelGenerationConsumerKind::Environment ||
		InConsumer.SpatialKind != EVoxelGenerationSpatialKind::Bounds ||
		InWidth <= 0 || InHeight <= 0 || InStep <= 0 || InWidth > 4096 || InHeight > 4096 / InWidth ||
		!FMath::IsFinite(InDistanceScore) || InDistanceScore < 0.0)
	{
		return {false, true, TEXT("InvalidRequest: map sample grid or consumer")};
	}
	const int64 MaxX = int64(InOrigin.X) + int64(InWidth - 1) * InStep + 1;
	const int64 MaxY = int64(InOrigin.Y) + int64(InHeight - 1) * InStep + 1;
	if (InOrigin.X < InConsumer.Min.X || InOrigin.Y < InConsumer.Min.Y ||
		MaxX > InConsumer.Max.X || MaxY > InConsumer.Max.Y)
	{
		return {false, true, TEXT("InvalidRequest: map sample batch outside original consumer")};
	}
	const FVoxelGenerationBounds Bounds{FIntVector(InOrigin.X, InOrigin.Y, InConsumer.Min.Z),
		FIntVector(int32(MaxX), int32(MaxY), InConsumer.Max.Z)};
	FExternalFactConsumer* Consumer = ExternalFactConsumers.Find(InConsumer);
	if (Consumer && Consumer->SampleStep == 0)
	{
		return {false, true, TEXT("InvalidConsumer: map samples cannot replace a terrain fact consumer")};
	}
	if (!Consumer)
	{
		FString Error;
		if (ExternalFactConsumers.Num() == MAX_int32 ||
			!ReserveFactStorage(WaitingPlanSections.Num(), ExternalFactConsumers.Num() + 1, Error))
		{
			if (Error.IsEmpty())
			{
				Error = TEXT("Oversized: map fact consumer directory");
			}
			return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		}
		Consumer = &ExternalFactConsumers.Add(InConsumer);
		Consumer->Bounds = Bounds;
		Consumer->SampleStep = InStep;
		FactStorageLease->MarkPublished();
	}
	else if (Consumer->Bounds.Min != Bounds.Min || Consumer->Bounds.Max != Bounds.Max || Consumer->SampleStep != InStep)
	{
		PlanCoordinator->ReleaseConsumer(InConsumer);
		Consumer = ExternalFactConsumers.Find(InConsumer);
		if (!Consumer)
		{
			return {};
		}
		Consumer->SampleRetentions.Empty();
		Consumer->SampleRetentionStorageLease.Reset();
		*Consumer = FExternalFactConsumer{};
		Consumer->Bounds = Bounds;
		Consumer->SampleStep = InStep;
	}
	const auto Resources = Scheduler.GetRuntimeResources();
	if (Consumer->bWaitingCapacity &&
		Resources->ReleaseVersionExcludingOwner(InConsumer) <= Consumer->CapacityVersion)
	{
		return {false, false, TEXT("CapacityBlocked: map dependency capture or retention")};
	}
	Consumer->bWaitingCapacity = false;
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InConsumer);
	FString Error;
	if (!Consumer->Dependencies.Lease &&
		!PlanCoordinator->GatherEnvironmentSamples(InOrigin, InWidth, InHeight, InStep,
			InConsumer, Consumer->Dependencies, Error))
	{
		Consumer->bWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
		Consumer->CapacityVersion = BeforeAdmission;
		return {false, !Consumer->bWaitingCapacity, MoveTemp(Error)};
	}
	const int32 DependencyCount = Consumer->Dependencies.Values.Num();
	if (DependencyCount > 64)
	{
		return {false, true, TEXT("InvalidRequest: map sample batch exceeds active dependency limit")};
	}
	if (!Consumer->SampleRetentionStorageLease)
	{
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<
			TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>, FDefaultAllocator>(DependencyCount);
		const auto Admission = Resources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes,
			Consumer->SampleRetentionStorageLease, 0, Consumer->Dependencies.Lease->Bytes() + FactStorageLease->Bytes(), 2);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Consumer->bWaitingCapacity = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
			Consumer->CapacityVersion = BeforeAdmission;
			return {false, !Consumer->bWaitingCapacity, Consumer->bWaitingCapacity
				? TEXT("CapacityBlocked: map retention directory") : TEXT("CapacityUnachievable: map retention directory")};
		}
		Consumer->SampleRetentions.Reserve(DependencyCount);
		check(Consumer->SampleRetentions.GetAllocatedSize() <= Consumer->SampleRetentionStorageLease->Bytes());
	}
	struct FMapRetentionSources
	{
		const FVoxelEmergeManager* Manager;
		FVoxelGenerationConsumerKey Consumer;
	};
	const FMapRetentionSources CapturedSources{this, InConsumer};
	FVoxelGenerationRequiredSourceScope RetentionSources(&Resources.Get(), &CapturedSources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Sources = *static_cast<const FMapRetentionSources*>(InContext);
			FVoxelGenerationRequiredSourceScope::VisitLease(Sources.Manager->FactStorageLease.Get(), InVisit);
			const auto* Capture = Sources.Manager->ExternalFactConsumers.Find(Sources.Consumer);
			if (!Capture) return;
			FVoxelGenerationRequiredSourceScope::VisitLease(Capture->Dependencies.Lease.Get(), InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Capture->SampleRetentionStorageLease.Get(), InVisit);
			for (const auto& Retention : Capture->SampleRetentions)
			{
				FVoxelGenerationRequiredSourceScope::VisitLease(&Retention->GetResourceLease().Get(), InVisit);
			}
		});
	const int32 RetentionEnd = FMath::Min(DependencyCount, Consumer->SampleRetentions.Num() + 8);
	const auto& Settings = Generator->GetRuntimeConfig()->Recipe->Settings;
	const int64 HydrologySide = int64(FMath::Max(8, Settings.HydrologyRegionSide)) * FMath::Max(1, Settings.HydrologyCellSize);
	while (Consumer->SampleRetentions.Num() < RetentionEnd)
	{
		const auto& Dependency = Consumer->Dependencies.Values[Consumer->SampleRetentions.Num()];
		const int64 CenterX = int64(Dependency.HydrologyKey.Coordinate.X) * HydrologySide + HydrologySide / 2;
		const int64 CenterY = int64(Dependency.HydrologyKey.Coordinate.Y) * HydrologySide + HydrologySide / 2;
		FVoxelGenerationCacheRetentionPoint Point;
		Point.Center = FIntPoint(int32(FMath::Clamp<int64>(CenterX, MIN_int32, MAX_int32)),
			int32(FMath::Clamp<int64>(CenterY, MIN_int32, MAX_int32)));
		const int64 RetentionRadius = FMath::Max(FMath::Abs(CenterX - Point.Center.X), FMath::Abs(CenterY - Point.Center.Y));
		if (RetentionRadius > MAX_int32)
		{
			return {false, true, TEXT("InvalidRequest: map hydrology retention outside supported world domain")};
		}
		Point.HydrologyRadiusCells = int32(RetentionRadius);
		Point.NaturalRadiusCells = Point.PlanRadiusCells = -1;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
		if (!Generator->GetPlanCache()->RetainForTask(Point, Resources, InConsumer, Retention, Error))
		{
			Consumer->bWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
			Consumer->CapacityVersion = BeforeAdmission;
			return {false, !Consumer->bWaitingCapacity, MoveTemp(Error)};
		}
		Consumer->SampleRetentions.Add(MoveTemp(Retention));
		Consumer->SampleRetentionStorageLease->MarkPublished();
	}
	if (Consumer->SampleRetentions.Num() < DependencyCount)
	{
		bOutNeedsAdvance = true;
		return {};
	}
	if (Consumer->SampleDependencyCursor == Consumer->Dependencies.Values.Num())
	{
		return {true, false, {}};
	}
	const int32 Cursor = Consumer->SampleDependencyCursor;
	const int32 Count = FMath::Min(8, Consumer->Dependencies.Values.Num() - Cursor);
	const auto Dependencies = MakeArrayView(Consumer->Dependencies.Values).Slice(Cursor, Count);
	const auto Status = PlanCoordinator->Ensure(Dependencies, EVoxelWorkClass::Background,
		0, InDistanceScore, 0.0, InConsumer);
	Consumer = ExternalFactConsumers.Find(InConsumer);
	if (!Consumer || Consumer->Bounds.Min != Bounds.Min || Consumer->Bounds.Max != Bounds.Max ||
		Consumer->SampleStep != InStep || Consumer->SampleDependencyCursor != Cursor)
	{
		return {};
	}
	if (!Status.bReady || Status.bFailed)
	{
		Consumer->bWaitingCapacity = Status.Error.StartsWith(TEXT("CapacityBlocked"));
		Consumer->CapacityVersion = BeforeAdmission;
		return Status;
	}
	Consumer->SampleDependencyCursor += Count;
	bOutNeedsAdvance = Consumer->SampleDependencyCursor < Consumer->Dependencies.Values.Num();
	return {!bOutNeedsAdvance, false, {}};
}

void FVoxelEmergeManager::Reset()
{
	TGuardValue<bool> ResetGuard(bResetting, true);
	Scheduler.CancelMatching([Owner = GenerationOwnerId, Epoch = Runtime.Epoch()](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return (Kind == EVoxelTaskKind::GenerateExactBase || Kind == EVoxelTaskKind::DecodeOverlay) &&
			Stamp.OwnerId == Owner && Stamp.WorldEpoch == Epoch;
	});
	check(EnvironmentTaskIncarnation != MAX_uint64);
	const uint64 PreviousIncarnation = EnvironmentTaskIncarnation++;
	Scheduler.CancelMatching([Owner = EnvironmentTaskOwnerId, PreviousIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::ProjectBackground && Stamp.OwnerId == Owner && Stamp.OwnerIncarnation == PreviousIncarnation;
	});
	for (const auto& Pair : ExternalFactConsumers) Generator->ReleaseOverlayConsumer(Pair.Key);
	for (const auto& Pair : WaitingPlanSections) Generator->ReleaseOverlayConsumer(Pair.Value.Consumer);
	ExternalFactConsumers.Reset();
	verify(AdmissionSelection.Reset());
	AdmissionAges.Empty();
	AdmissionAgeStorageLease.Reset();
	AdmissionCostEstimateMicroseconds = 25.0;
	CurrentDemand.Empty();
	for (const auto& Pair : ExternalDataConsumers)
	{
		verify(Runtime.ReleaseSectionPin(Pair.Value.Section, Pair.Value.Pin.Resource));
	}
	ExternalDataDemands.Empty();
	ExternalDataOrder.Empty();
	ExternalDataConsumers.Empty();
	ExternalDataStorageLease.Reset();
	ExternalDataCapacity = ExternalDataConsumerCapacity = ExternalDataCursor = 0;
	ExternalDataAdmission = {};
	CriticalAdmissionKeys.Empty();
	ReadyCriticalSections.Empty();
	ReadyCriticalSections.Values.Empty();
	QueuedCriticalSections.Empty();
	CriticalFallbackCursor = 0;
	WaitingPlanSections.Empty();
	ExternalFactConsumers.Empty();
	FactStorageLease.Reset();
	WaitingFactCapacity = 0;
	ExternalFactCapacity = 0;
	FactStorageAdmission = {};
	EnvironmentColumnStates.Empty();
	EnvironmentStorageLease.Reset();
	PlanCoordinator->Reset();
	RemoteChangeStates.Empty();
	RemoteChangeStorageLease.Reset();
	RemoteChangeAdmission = {};
	OrderedKeys.Empty();
	for (auto& Lane : AdmissionLanes)
	{
		Lane.Empty();
	}
	DemandStorageLease.Reset();
	ClearDemandRebuild();
	LastResourceAdmission = EVoxelTerrainResourceAdmission::None;
	bTickWaitingCapacity = false;
	TickCapacityVersion = 0;

	CurrentInterestRevision = 0;
	CurrentPriorityRevision = 0;
	NextAdmissionIndices[0] = 0;
	NextAdmissionIndices[1] = 0;
	LastAdmissionDiagnosticTime = 0.0;
	PlanPendingAttempts = 0;
	PlanReadyAttempts = 0;
	OverlayPendingAttempts = 0;
	OverlayReadyAttempts = 0;
	EnqueueAttempts = 0;
	EnqueuedTasks = 0;
}

void FVoxelEmergeManager::SetRemoteOverlayResolver(TFunction<bool(const FIntVector&, uint64&,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> InResolver)
{
	RemoteOverlayResolver = MoveTemp(InResolver);
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::EnsureGenerationFacts(
	const FVoxelGenerationConsumerKey& InConsumer, const FVoxelGenerationBounds& InBounds,
	const FVoxelTerrainServicePriority& InPriority)
{
	check(IsInGameThread());
	if (!InConsumer.IsValid() || InConsumer.WorldEpoch != Runtime.Epoch() || !InBounds.IsValid())
	{
		return {false, true, TEXT("外部事实消费者缺少完整身份或范围")};
	}
	FExternalFactConsumer* Consumer = ExternalFactConsumers.Find(InConsumer);
	if (Consumer && (Consumer->Bounds.Min != InBounds.Min || Consumer->Bounds.Max != InBounds.Max))
	{
		return {false, true, TEXT("同一事实Consumer不能改变捕获范围")};
	}
	if (!Consumer)
	{
		if (ExternalFactConsumers.Num() == MAX_int32)
		{
			FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return {false, true, TEXT("Oversized: generation fact consumer directory")};
		}
		FString Error;
		if (!ReserveFactStorage(WaitingPlanSections.Num(), ExternalFactConsumers.Num() + 1, Error))
			return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		FExternalFactConsumer Candidate;
		Candidate.Bounds = InBounds;
		Consumer = &ExternalFactConsumers.Add(InConsumer, MoveTemp(Candidate));
		FactStorageLease->MarkPublished();
	}
	const auto Resources = Scheduler.GetRuntimeResources();
	if (Consumer->bWaitingCapacity && Resources->ReleaseVersionExcludingOwner(InConsumer) <= Consumer->CapacityVersion)
		return {false, false, TEXT("CapacityBlocked: generation fact capture or retention")};
	Consumer->bWaitingCapacity = false;
	const uint64 Version = Resources->ReleaseVersionExcludingOwner(InConsumer);
	if (!Consumer->Dependencies.Lease)
	{
		FString Error;
		if (!PlanCoordinator->GatherForBounds(InBounds, InConsumer, Consumer->Dependencies, Error))
		{
			Consumer->bWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
			Consumer->CapacityVersion = Version;
			return {false, !Consumer->bWaitingCapacity, MoveTemp(Error)};
		}
	}
	if (!Consumer->Retention)
	{
		FString Error;
		if (!PlanCoordinator->RetainDependencies(Consumer->Dependencies.Values,
			FIntPoint(int32((int64(InBounds.Min.X) + InBounds.Max.X) / 2), int32((int64(InBounds.Min.Y) + InBounds.Max.Y) / 2)),
			InConsumer, Consumer->Retention, Error))
		{
			Consumer->bWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
			Consumer->CapacityVersion = Version;
			return {false, !Consumer->bWaitingCapacity, MoveTemp(Error)};
		}
	}
	const EVoxelWorkClass WorkClass = InPriority.Band <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	const auto Natural = PlanCoordinator->Ensure(Consumer->Dependencies.Values, WorkClass, InPriority, InConsumer);
	const auto Overlay = Generator->EnsureOverlayConsumerReady(InConsumer, InBounds, WorkClass, InPriority);
	if (Natural.bFailed) return Natural;
	if (Overlay.bFailed) return {false, true, Overlay.Error};
	return {Natural.bReady && Overlay.bReady, false, {}};
}

void FVoxelEmergeManager::ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	PlanCoordinator->ReleaseConsumer(InConsumer);
	Generator->ReleaseOverlayConsumer(InConsumer);
	ExternalFactConsumers.Remove(InConsumer);
}

void FVoxelEmergeManager::SetExternalConsumerWakeCallback(TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback)
{
	check(IsInGameThread());
	ExternalConsumerWakeCallback = MoveTemp(InCallback);
}

bool FVoxelEmergeManager::ReserveFactStorage(const int32 InWaiting, const int32 InExternal, FString& OutError)
{
	const auto Resources = Scheduler.GetRuntimeResources();
	if (InWaiting < 0 || InExternal < 0)
	{
		FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		OutError = TEXT("InvalidRequest: generation fact consumer storage count");
		return false;
	}
	FInputBudget Budget(Resources, GetAdmissionConsumer(), FactStorageLease,
		FactStorageAdmission.Admission, FactStorageAdmission.CapacityVersion);
	if (!Budget.Check(OutError))
	{
		return false;
	}
	if (VoxelGenerationAllocation::Private::SetCanReserve<TPair<FIntVector, FWaitingPlanSection>>(WaitingPlanSections, InWaiting) &&
		VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelGenerationConsumerKey, FExternalFactConsumer>>(
			ExternalFactConsumers, InExternal))
	{
		WaitingFactCapacity = FMath::Max(WaitingFactCapacity, InWaiting);
		ExternalFactCapacity = FMath::Max(ExternalFactCapacity, InExternal);
		FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::Granted;
		OutError.Reset();
		return true;
	}
	if (!CanRetryStorage(FactStorageAdmission, *Resources,
		TEXT("CapacityBlocked: generation fact consumer storage awaiting external release"), OutError))
	{
		FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
	}
	const int32 Waiting = InWaiting > WaitingFactCapacity ? int32(FMath::Max<int64>(InWaiting,
		FMath::Min<int64>(MAX_int32, FMath::Max<int64>(4, int64(WaitingFactCapacity) * 2)))) : WaitingFactCapacity;
	const int32 External = InExternal > ExternalFactCapacity ? int32(FMath::Max<int64>(InExternal,
		FMath::Min<int64>(MAX_int32, FMath::Max<int64>(4, int64(ExternalFactCapacity) * 2)))) : ExternalFactCapacity;
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Manager = *static_cast<const FVoxelEmergeManager*>(InContext);
			Manager.VisitStorageSources(*Manager.Scheduler.GetRuntimeResources(), InVisit);
		});
	if (!VoxelGenerationAllocation::Private::ReserveSet<TPair<FIntVector, FWaitingPlanSection>>(
		&Budget, WaitingPlanSections, Waiting, OutError) ||
		!VoxelGenerationAllocation::Private::ReserveSet<TPair<FVoxelGenerationConsumerKey, FExternalFactConsumer>>(
			&Budget, ExternalFactConsumers, External, OutError))
	{
		FactStorageAdmission.bWaitingCapacity = FactStorageAdmission.Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
	}
	if (FactStorageLease && (!WaitingPlanSections.IsEmpty() || !ExternalFactConsumers.IsEmpty()))
	{
		FactStorageLease->MarkPublished();
	}
	WaitingFactCapacity = Waiting;
	ExternalFactCapacity = External;
	FactStorageAdmission.bWaitingCapacity = false;
	FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::Granted;
	OutError.Reset();
	return true;
}

bool FVoxelEmergeManager::ReserveExternalDataStorage(const int32 InSectionCount, const int32 InConsumerCount, FString& OutError)
{
	const auto Resources = Scheduler.GetTerrainResources();
	if (InSectionCount < 0 || InConsumerCount < 0)
	{
		ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		OutError = TEXT("InvalidRequest: external data consumer storage count");
		return false;
	}
	FInputBudget Budget(Resources, GetAdmissionConsumer(), ExternalDataStorageLease,
		ExternalDataAdmission.Admission, ExternalDataAdmission.CapacityVersion);
	if (!Budget.Check(OutError))
	{
		return false;
	}
	if (VoxelGenerationAllocation::Private::SetCanReserve<TPair<FIntVector, FExternalDataDemand>>(
		ExternalDataDemands, InSectionCount) && FInputBudget::SparseCanReserve(ExternalDataOrder, InSectionCount) &&
		VoxelGenerationAllocation::Private::SetCanReserve<TPair<FGuid, FExternalDataConsumer>>(ExternalDataConsumers, InConsumerCount))
	{
		if (!Budget.ReserveSparse(ExternalDataOrder, InSectionCount, OutError))
		{
			return false;
		}
		ExternalDataCapacity = FMath::Max(ExternalDataCapacity, InSectionCount);
		ExternalDataConsumerCapacity = FMath::Max(ExternalDataConsumerCapacity, InConsumerCount);
		ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::Granted;
		OutError.Reset();
		return true;
	}
	if (!CanRetryStorage(ExternalDataAdmission, *Resources,
		TEXT("CapacityBlocked: external data consumer storage awaiting external release"), OutError))
	{
		ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
	}
	const int32 Rows = int32(FMath::Max<int64>(16, InSectionCount > ExternalDataCapacity ?
		FMath::Max<int64>(InSectionCount, FMath::Min<int64>(MAX_int32, int64(ExternalDataCapacity) * 2)) : ExternalDataCapacity));
	const int32 ConsumerRows = int32(FMath::Max<int64>(16, InConsumerCount > ExternalDataConsumerCapacity ?
		FMath::Max<int64>(InConsumerCount, FMath::Min<int64>(MAX_int32, int64(ExternalDataConsumerCapacity) * 2)) : ExternalDataConsumerCapacity));
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Manager = *static_cast<const FVoxelEmergeManager*>(InContext);
			Manager.VisitStorageSources(*Manager.Scheduler.GetTerrainResources(), InVisit);
		});
	if (!VoxelGenerationAllocation::Private::ReserveSet<TPair<FIntVector, FExternalDataDemand>>(
		&Budget, ExternalDataDemands, Rows, OutError) || !Budget.ReserveSparse(ExternalDataOrder, Rows, OutError) ||
		!VoxelGenerationAllocation::Private::ReserveSet<TPair<FGuid, FExternalDataConsumer>>(
			&Budget, ExternalDataConsumers, ConsumerRows, OutError))
	{
		ExternalDataAdmission.bWaitingCapacity = ExternalDataAdmission.Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
	}
	if (!ExternalDataDemands.IsEmpty() || !ExternalDataConsumers.IsEmpty())
	{
		ExternalDataStorageLease->MarkPublished();
	}
	ExternalDataCapacity = Rows;
	ExternalDataConsumerCapacity = ConsumerRows;
	ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::Granted;
	ExternalDataAdmission.bWaitingCapacity = false;
	OutError.Reset();
	return true;
}

bool FVoxelEmergeManager::AcquireDataDemand(const FIntVector& InSection, FVoxelRuntimeSectionPin& OutPin)
{
	check(IsInGameThread());
	if (bResetting)
	{
		ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	auto* Existing = ExternalDataDemands.Find(InSection);
	FString Error;
	if ((Existing && Existing->References == MAX_int32) || (!Existing && ExternalDataDemands.Num() == MAX_int32) ||
		ExternalDataConsumers.Num() == MAX_int32)
	{
		ExternalDataAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	if (!ReserveExternalDataStorage(ExternalDataDemands.Num() + (Existing ? 0 : 1), ExternalDataConsumers.Num() + 1, Error))
	{
		return false;
	}
	// Reserve 可搬移聚合表；其字段只能在新的地址上读取。
	Existing = ExternalDataDemands.Find(InSection);
	auto* Section = Runtime.FindOrAllocate(InSection, GFrameCounter);
	if (!Section) return false;
	const auto Identity = Section->GetResourceIdentity();
	if ((Existing && Existing->Identity != Identity) || !Runtime.TryPinSection(InSection, Identity)) return false;
	if (!Existing)
	{
		FExternalDataDemand Record;
		Record.Identity = Identity;
		Record.Demand.bData = true;
		Record.Demand.bExact = true;
		Record.Demand.ServicePriority.Band = EVoxelTerrainServiceBand::P2;
		Record.Demand.ServicePriority.SourceStableKey = GenerationOwnerId;
		Record.OrderIndex = ExternalDataOrder.Add(InSection);
		Existing = &ExternalDataDemands.Add(InSection, MoveTemp(Record));
	}
	++Existing->References;
	OutPin = {Identity, FGuid::NewGuid()};
	ExternalDataConsumers.Add(OutPin.ConsumerId, {InSection, OutPin});
	ExternalDataStorageLease->MarkPublished();
	return true;
}

bool FVoxelEmergeManager::ReleaseDataDemand(const FIntVector& InSection, const FVoxelRuntimeSectionPin& InPin)
{
	check(IsInGameThread());
	auto* Demand = ExternalDataDemands.Find(InSection);
	const auto* Consumer = ExternalDataConsumers.Find(InPin.ConsumerId);
	const auto* Section = Runtime.FindSection(InSection);
	if (!Consumer || Consumer->Section != InSection || Consumer->Pin != InPin || !Demand ||
		Demand->Identity != InPin.Resource || Demand->References <= 0 || !Section ||
		Section->GetResourceIdentity() != InPin.Resource) return false;
	ExternalDataConsumers.Remove(InPin.ConsumerId);
	const bool bLast = --Demand->References == 0;
	if (bLast)
	{
		ExternalDataOrder.RemoveAt(Demand->OrderIndex);
		ExternalDataDemands.Remove(InSection);
		if (!CurrentDemand.Contains(InSection))
		{
			if (const auto* Waiting = WaitingPlanSections.Find(InSection))
			{
				PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
				Generator->ReleaseOverlayConsumer(Waiting->Consumer);
				WaitingPlanSections.Remove(InSection);
			}
			AdmissionAges.Remove(InSection);
			Scheduler.CancelMatching([this, InSection](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return (Kind == EVoxelTaskKind::GenerateExactBase || Kind == EVoxelTaskKind::DecodeOverlay) &&
					Stamp.OwnerId == GenerationOwnerId && Stamp.WorldEpoch == Runtime.Epoch() && Stamp.Section == InSection;
			});
		}
	}
	verify(Runtime.ReleaseSectionPin(InSection, InPin.Resource));
	return true;
}

const FVoxelExactDemand* FVoxelEmergeManager::FindDemand(const FIntVector& InSection) const
{
	const auto* Interest = CurrentDemand.Find(InSection);
	const auto* External = ExternalDataDemands.Find(InSection);
	if (External && (!Interest || FVoxelTerrainWorkSelection::HigherPriority(External->Demand.ServicePriority, Interest->ServicePriority)))
		return &External->Demand;
	return Interest;
}

EVoxelWorkClass FVoxelEmergeManager::ResolveWorkClass(const FVoxelExactDemand& InDemand)
{
	if (IsCriticalDemand(InDemand))
	{
		return EVoxelWorkClass::Critical;
	}

	return InDemand.bExact || InDemand.bCollision || InDemand.bSimulation ? EVoxelWorkClass::ExactData : EVoxelWorkClass::Visible;
}

bool FVoxelEmergeManager::RebuildDemand(const FVoxelInterestSet& InInterest,
	const uint64 InInterestRevision, const double InDeadline)
{
	if (!DemandRebuild.bActive)
	{
		DemandRebuild.bActive = true;
		DemandRebuild.InterestRevision = InInterestRevision;
		DemandRebuild.PriorityRevision = InInterest.PriorityRevision;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(GetAdmissionConsumer());
	FString Error;
	if (DemandRebuild.Phase < 7 && InInterest.ExactOrder.Num() != InInterest.Exact.Num())
	{
		return DeferTickAdmission(BeforeAdmission, TEXT("InvalidRequest: emerge demand order coverage"));
	}
	while (FPlatformTime::Seconds() < InDeadline)
	{
		if (DemandRebuild.Phase == 0)
		{
			FInputBudget Budget(Resources, GetAdmissionConsumer(), DemandRebuild.Lease, LastResourceAdmission, BeforeAdmission);
			bool bReserved = false;
			switch (DemandRebuild.Cursor)
			{
			case 0:
				bReserved = VoxelGenerationAllocation::Private::ReserveSet<TPair<FIntVector, FVoxelExactDemand>>(
					&Budget, DemandRebuild.Demand, InInterest.Exact.Num(), Error);
				break;
			case 1:
				bReserved = VoxelGenerationAllocation::Private::ReserveArray(&Budget, DemandRebuild.Order, InInterest.ExactOrder.Num(), Error);
				break;
			case 2:
			case 3:
				bReserved = VoxelGenerationAllocation::Private::ReserveArray(&Budget, DemandRebuild.Lanes[DemandRebuild.Cursor - 2],
					InInterest.ExactAdmissionLanes[DemandRebuild.Cursor - 2].Num(), Error);
				break;
			case 4:
				bReserved = VoxelGenerationAllocation::Private::ReserveArray(&Budget, DemandRebuild.Critical, InInterest.ExactOrder.Num(), Error);
				break;
			case 5:
				bReserved = VoxelGenerationAllocation::Private::ReserveArray(&Budget, DemandRebuild.Ready.Values, InInterest.ExactOrder.Num(), Error);
				break;
			case 6:
				bReserved = VoxelGenerationAllocation::Private::ReserveSet<FIntVector>(&Budget, DemandRebuild.Queued, InInterest.ExactOrder.Num(), Error);
				break;
			default:
				DemandRebuild.Ready.Values.SetNumUninitialized(InInterest.ExactOrder.Num());
				DemandRebuild.Phase = 1;
				DemandRebuild.Cursor = 0;
				continue;
			}
			if (!bReserved)
			{
				return DeferTickAdmission(BeforeAdmission, Error);
			}
			++DemandRebuild.Cursor;
			continue;
		}
		if (DemandRebuild.Phase == 1)
		{
			if (DemandRebuild.Cursor == InInterest.ExactOrder.Num())
			{
				DemandRebuild.Phase = 2;
				DemandRebuild.Cursor = 0;
				continue;
			}
			const FIntVector Key = InInterest.ExactOrder[DemandRebuild.Cursor];
			const auto* Demand = InInterest.Exact.Find(Key);
			if (!Demand || DemandRebuild.Demand.Contains(Key))
			{
				return DeferTickAdmission(BeforeAdmission, TEXT("InvalidRequest: emerge demand order identity"));
			}
			DemandRebuild.Demand.Add(Key, *Demand);
			DemandRebuild.Order.Add(Key);
			if (IsCriticalDemand(*Demand))
			{
				DemandRebuild.Critical.Add(Key);
			}
			++DemandRebuild.Cursor;
			continue;
		}
		if (DemandRebuild.Phase == 2 || DemandRebuild.Phase == 3)
		{
			const int32 Lane = DemandRebuild.Phase - 2;
			if (DemandRebuild.Cursor == InInterest.ExactAdmissionLanes[Lane].Num())
			{
				++DemandRebuild.Phase;
				DemandRebuild.Cursor = 0;
				continue;
			}
			const int32 Index = InInterest.ExactAdmissionLanes[Lane][DemandRebuild.Cursor];
			if (!DemandRebuild.Order.IsValidIndex(Index))
			{
				return DeferTickAdmission(BeforeAdmission, TEXT("InvalidRequest: emerge admission lane index"));
			}
			DemandRebuild.Lanes[Lane].Add(Index);
			++DemandRebuild.Cursor;
			continue;
		}
		if (DemandRebuild.Phase == 4 || DemandRebuild.Phase == 5)
		{
			const int32 Count = DemandRebuild.Phase == 4 ? ReadyCriticalSections.Num() : DemandRebuild.Critical.Num();
			if (DemandRebuild.Cursor == Count)
			{
				++DemandRebuild.Phase;
				DemandRebuild.Cursor = 0;
				continue;
			}
			const FIntVector Key = DemandRebuild.Phase == 4 ? ReadyCriticalSections.At(DemandRebuild.Cursor) :
				DemandRebuild.Critical[DemandRebuild.Cursor];
			const auto* Demand = DemandRebuild.Demand.Find(Key);
			if (Demand && IsCriticalDemand(*Demand) && !DemandRebuild.Queued.Contains(Key))
			{
				const bool bEnqueued = DemandRebuild.Ready.Enqueue(Key);
				check(bEnqueued);
				DemandRebuild.Queued.Add(Key);
			}
			++DemandRebuild.Cursor;
			continue;
		}
		if (DemandRebuild.Phase == 6)
		{
			const bool bSameDemandRevision = CurrentInterestRevision == DemandRebuild.InterestRevision;
			Swap(CurrentDemand, DemandRebuild.Demand);
			Swap(OrderedKeys, DemandRebuild.Order);
			for (int32 Lane = 0; Lane < 2; ++Lane)
			{
				Swap(AdmissionLanes[Lane], DemandRebuild.Lanes[Lane]);
			}
			Swap(CriticalAdmissionKeys, DemandRebuild.Critical);
			Swap(ReadyCriticalSections, DemandRebuild.Ready);
			Swap(QueuedCriticalSections, DemandRebuild.Queued);
			Swap(DemandStorageLease, DemandRebuild.Lease);
			if (DemandStorageLease)
			{
				DemandStorageLease->MarkPublished();
			}
			CurrentInterestRevision = DemandRebuild.InterestRevision;
			CurrentPriorityRevision = DemandRebuild.PriorityRevision;
			NextAdmissionIndices[0] = !bSameDemandRevision || AdmissionLanes[0].IsEmpty() ? 0 :
				NextAdmissionIndices[0] % AdmissionLanes[0].Num();
			NextAdmissionIndices[1] = AdmissionLanes[1].IsEmpty() ? 0 : NextAdmissionIndices[1] % AdmissionLanes[1].Num();
			CriticalFallbackCursor = CriticalAdmissionKeys.IsEmpty() ? 0 : CriticalFallbackCursor % CriticalAdmissionKeys.Num();
			DemandRebuild.Phase = 7;
			DemandRebuild.Cursor = 0;
			continue;
		}
		if (DemandRebuild.Phase == 7)
		{
			if (DemandRebuild.Cursor == DemandRebuild.Order.Num())
			{
				DemandRebuild.Phase = 8;
				DemandRebuild.Cursor = 0;
				continue;
			}
			const FIntVector Key = DemandRebuild.Order[DemandRebuild.Cursor++];
			if (!FindDemand(Key))
			{
				AdmissionAges.Remove(Key);
				if (const auto* Waiting = WaitingPlanSections.Find(Key))
				{
					const auto Consumer = Waiting->Consumer;
					WaitingPlanSections.Remove(Key);
					PlanCoordinator->ReleaseConsumer(Consumer);
					Generator->ReleaseOverlayConsumer(Consumer);
				}
			}
			continue;
		}
		if (DemandRebuild.Phase == 8)
		{
			if (DemandRebuild.Cursor == OrderedKeys.Num())
			{
				DemandRebuild.Phase = 9;
				continue;
			}
			const FIntVector Key = OrderedKeys[DemandRebuild.Cursor++];
			const FVoxelExactDemand* Previous = DemandRebuild.Demand.Find(Key);
			const FVoxelExactDemand* Next = FindDemand(Key);
			const auto* Waiting = WaitingPlanSections.Find(Key);
			if (Previous && Next && Waiting && Waiting->Dependencies.Lease &&
				(ResolveWorkClass(*Previous) != ResolveWorkClass(*Next) || Previous->ServicePriority != Next->ServicePriority))
			{
				const auto Consumer = Waiting->Consumer;
				const FVoxelExactDemand Demand = *Next;
				PlanCoordinator->Ensure(Waiting->Dependencies.Values, ResolveWorkClass(Demand), Demand.ServicePriority, Consumer);
				const FIntVector Min = Key * 16;
				Generator->EnsureOverlayConsumerReady(Consumer, {Min, Min + FIntVector(16)}, ResolveWorkClass(Demand), Demand.ServicePriority);
			}
			continue;
		}
		ClearDemandRebuild();
		Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return (Kind == EVoxelTaskKind::GenerateExactBase || Kind == EVoxelTaskKind::DecodeOverlay) &&
				Stamp.OwnerId == GenerationOwnerId && Stamp.WorldEpoch == Runtime.Epoch() && !FindDemand(Stamp.Section);
		});
		Scheduler.UpdateServicePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
			EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
		{
			if (Kind != EVoxelTaskKind::GenerateExactBase && Kind != EVoxelTaskKind::DecodeOverlay)
			{
				return;
			}
			if (const FVoxelExactDemand* Demand = FindDemand(Stamp.Section))
			{
				WorkClass = ResolveWorkClass(*Demand);
				Priority = Demand->ServicePriority;
			}
		});
		LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	return false;
}

bool FVoxelEmergeManager::RequestSection(
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	if (bResetting) return false;
	FVoxelSection* Section =
		Runtime.FindOrAllocate(
			InKey,
			GFrameCounter);

	if (!Section)
	{
		return false;
	}
	if (Section->PendingPublish && Section->PendingPublish->Kind != EVoxelTaskKind::None)
	{
		const FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InKey);
		const auto& PendingStamp = Section->PendingPublish->Stamp;
		if (!Waiting || Waiting->Stamp != Section->Stamp || PendingStamp.WorldEpoch != Runtime.Epoch() ||
			PendingStamp.OwnerId != GenerationOwnerId || PendingStamp.OwnerIncarnation != Section->Stamp.Token ||
			PendingStamp.Token != Section->Stamp.Token || PendingStamp.Revision != Waiting->Consumer.RequestSerial)
		{
			*Section->PendingPublish = FVoxelTaskResult();
			return false;
		}
		if (Runtime.GetTerrainResources()->ReleaseVersionExcludingOwner(Waiting->Consumer) <= Section->PendingCapacityVersion) return false;
		auto Pending = MoveTemp(*Section->PendingPublish);
		*Section->PendingPublish = FVoxelTaskResult();
		OnTask(MoveTemp(Pending));
		return Section->PendingPublish->Kind == EVoxelTaskKind::None;
	}

	switch (Section->Status)
	{
	case EVoxelSectionStatus::Allocated:
		return RequestBase(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::BaseReady:
		return ResolveOverlay(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::DataReady:
		Section->bSimulationWanted =
			InDemand.bSimulation;
		break;

	case EVoxelSectionStatus::None:
	case EVoxelSectionStatus::Failed:
	case EVoxelSectionStatus::Unloading:
	default:
		break;
	}
	return false;
}

FVoxelEmergeManager::FWaitingPlanSection* FVoxelEmergeManager::FindOrCreateWaiting(
	FVoxelSection& InSection, const FIntVector& InKey)
{
	const FVoxelSectionStamp ExpectedStamp = InSection.Stamp;
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InKey);
	if (Waiting && Waiting->Stamp != ExpectedStamp)
	{
		const FVoxelSectionStamp Previous = Waiting->Stamp;
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		Scheduler.CancelMatching([Owner = GenerationOwnerId, Previous, InKey](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return (Kind == EVoxelTaskKind::GenerateExactBase || Kind == EVoxelTaskKind::DecodeOverlay) &&
				Stamp.OwnerId == Owner && Stamp.WorldEpoch == Previous.Epoch && Stamp.Token == Previous.Token &&
				Stamp.OwnerIncarnation == Previous.Token && Stamp.Section == InKey;
		});
		Waiting = nullptr;
	}
	if (Waiting) return Waiting;
	FVoxelSection* CurrentSection = Runtime.FindSection(InKey);
	if (!CurrentSection || CurrentSection->Stamp != ExpectedStamp || !FindDemand(InKey)) return nullptr;
	Waiting = WaitingPlanSections.Find(InKey);
	if (Waiting) return Waiting->Stamp == ExpectedStamp ? Waiting : nullptr;
	if (WaitingPlanSections.Num() == MAX_int32)
	{
		FactStorageAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		CurrentSection->Status = EVoxelSectionStatus::Failed;
		CurrentSection->Error = TEXT("Oversized: exact generation consumer directory");
		return nullptr;
	}
	FString Error;
	if (!ReserveFactStorage(WaitingPlanSections.Num() + 1, ExternalFactConsumers.Num(), Error))
	{
		if (!Error.StartsWith(TEXT("CapacityBlocked")))
		{
			CurrentSection->Status = EVoxelSectionStatus::Failed;
			CurrentSection->Error = MoveTemp(Error);
		}
		return nullptr;
	}
	if (NextGenerationRequestSerial == MAX_uint64)
	{
		CurrentSection->Status = EVoxelSectionStatus::Failed;
		CurrentSection->Error = TEXT("Oversized: exact generation consumer serial");
		return nullptr;
	}
	Waiting = &WaitingPlanSections.Add(InKey);
	Waiting->Stamp = ExpectedStamp;
	Waiting->Consumer = FVoxelGenerationConsumerKey::Section(ExpectedStamp.Epoch, GenerationOwnerId,
		ExpectedStamp.Token, InKey, ++NextGenerationRequestSerial);
	Waiting->BlockedSinceSeconds = FPlatformTime::Seconds();
	FactStorageLease->MarkPublished();
	return Waiting;
}

bool FVoxelEmergeManager::DeferExactAdmission(FVoxelSection& InSection, const FIntVector& InKey,
	FWaitingPlanSection& InWaiting, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const uint64 InBeforeAdmission, const FString& InError)
{
	const bool bTemporary = InError.StartsWith(TEXT("CapacityBlocked"));
	if (InWaiting.PendingRequest)
	{
		Scheduler.ObserveDemand(InWaiting.PendingRequest->Kind, InWaiting.PendingRequest->Stamp,
			bTemporary ? EVoxelTaskBlockReason::InputMemory :
			InError.StartsWith(TEXT("CapacityUnachievable")) || InError.StartsWith(TEXT("Oversized")) ?
				EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	}
	if (bTemporary)
	{
		InWaiting.AdmissionResources = InResources;
		InWaiting.AdmissionCapacityVersion = InBeforeAdmission;
		InWaiting.bWaitingTaskCapacity = true;
		return false;
	}
	InSection.Status = EVoxelSectionStatus::Failed;
	InSection.Error = InError;
	const FVoxelGenerationConsumerKey Consumer = InWaiting.Consumer;
	WaitingPlanSections.Remove(InKey);
	PlanCoordinator->ReleaseConsumer(Consumer);
	Generator->ReleaseOverlayConsumer(Consumer);
	return false;
}

bool FVoxelEmergeManager::SubmitExactRequest(FVoxelSection& InSection, const FIntVector& InKey,
	FWaitingPlanSection& InWaiting, const FVoxelExactDemand& InDemand)
{
	FVoxelTaskRequest& Request = InWaiting.PendingRequest.GetValue();
	const auto Resources = Scheduler.GetTerrainResources();
	uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
	const auto RejectGrant = [&](const EVoxelTerrainResourceAdmission Admission)
	{
		const FString Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: exact task backing") : Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("CapacityUnachievable: exact task backing") : TEXT("InvalidRequest: exact task backing");
		return DeferExactAdmission(InSection, InKey, InWaiting, Resources, BeforeAdmission, Error);
	};
	if (Request.Stamp.WorldEpoch != Runtime.Epoch() || !Runtime.IsCurrentStamp(InKey, InWaiting.Stamp) ||
		Request.GenerationConsumer != InWaiting.Consumer || Request.Stamp.Revision != InWaiting.Consumer.RequestSerial ||
		Request.Stamp.OwnerId != GenerationOwnerId ||
		Request.Stamp.OwnerIncarnation != InSection.Stamp.Token || Request.Stamp.Token != InSection.Stamp.Token ||
		Request.Stamp.Section != InKey)
	{
		return RejectGrant(EVoxelTerrainResourceAdmission::InvalidRequest);
	}
	Request.WorkClass = ResolveWorkClass(InDemand);
	Request.SourcePriority = InDemand.Priority;
	Request.ServiceBand = InDemand.ServicePriority.Band;
	Request.SourceStableKey = InDemand.ServicePriority.SourceStableKey;
	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;
	FString Error;
	if (Request.Kind == EVoxelTaskKind::GenerateExactBase)
	{
		if (!InWaiting.BaseWork)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			const auto Admission = Resources->TryAcquire(InWaiting.Consumer, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelExactBaseGenerationWork>(), Lease);
			if (Admission != EVoxelTerrainResourceAdmission::Granted) return RejectGrant(Admission);
			InWaiting.BaseWork = VoxelBoundedAllocation::MakeWorkerShared<FVoxelExactBaseGenerationWork>(Lease);
			InWaiting.BaseWork->MetadataLease = MoveTemp(Lease);
			InWaiting.BaseWork->Generator = Generator;
			InWaiting.BaseWork->Retention = InWaiting.Retention;
			InWaiting.BaseWork->FactResources = Scheduler.GetRuntimeResources();
			InWaiting.BaseWork->Consumer = InWaiting.Consumer;
			InWaiting.BaseWork->Section = InKey;
		}
		const auto Work = InWaiting.BaseWork;
		const uint64 BaseBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelBlockState>>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(4096);
		Request.SetRequiredBindingInput(Work->MetadataLease->Bytes(), 1 + int32(Work->BaseLease.IsValid()));
		if (!Work->BaseLease)
		{
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			const auto Admission = Resources->TryAcquire(InWaiting.Consumer, EVoxelTerrainResourcePool::Staged,
				BaseBytes, Work->BaseLease, 0, Work->MetadataLease->Bytes() + Request.GetBindingBytes(),
				1 + Request.GetBindingLeaseCount());
			if (Admission != EVoxelTerrainResourceAdmission::Granted) return RejectGrant(Admission);
			Request.SetRequiredBindingInput(Work->MetadataLease->Bytes(), 2);
		}
		Request.InputBytes = Work->MetadataLease->Bytes();
		Request.ReservedBytes = BaseBytes;
		if (!Request.bBlockBound)
		{
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			if (!Request.BindGetBlockReason(Resources, InWaiting.Consumer, [Work] { return Work->GetBlockReason(); }, Error))
				return DeferExactAdmission(InSection, InKey, InWaiting, Resources, BeforeAdmission, Error);
		}
		if (!Request.bExecuteBound)
		{
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			if (!Request.BindExecute(Resources, InWaiting.Consumer,
				[Work](const TAtomic<bool>& InCancel) { return Work->Advance(InCancel); }, Error))
				return DeferExactAdmission(InSection, InKey, InWaiting, Resources, BeforeAdmission, Error);
		}
	}
	else if (Request.Kind == EVoxelTaskKind::DecodeOverlay)
	{
		if (!InWaiting.OverlayWork)
		{
			if (RegionStore.GetSourceDirectory().Len() == MAX_int32) return RejectGrant(EVoxelTerrainResourceAdmission::CapacityUnachievable);
			const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelExactOverlayDecodeWork>() +
				VoxelBoundedAllocation::ArrayBytes<TCHAR>(RegionStore.GetSourceDirectory().Len() + 1);
			uint64 SourceInput = 0;
			int32 SourceCount = 0;
			const auto SourceLease = RegionStore.GetSourceLease() ? RegionStore.GetSourceLease()->GetResourceLease() : nullptr;
			if (SourceLease)
			{
				if (!SourceLease->IsOwnedBy(Resources.Get())) return RejectGrant(EVoxelTerrainResourceAdmission::InvalidRequest);
				SourceCount = 1;
				if (SourceLease->GetPool() == EVoxelTerrainResourcePool::Input) SourceInput = SourceLease->Bytes();
			}
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			const auto Admission = Resources->TryAcquire(InWaiting.Consumer, EVoxelTerrainResourcePool::Input,
				InputBytes, Lease, 0, SourceInput, SourceCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted) return RejectGrant(Admission);
			InWaiting.OverlayWork = VoxelBoundedAllocation::MakeWorkerShared<FVoxelExactOverlayDecodeWork>(Lease);
			InWaiting.OverlayWork->MetadataLease = MoveTemp(Lease);
			InWaiting.OverlayWork->Read = RegionStore.CaptureRead(InKey);
			InWaiting.OverlayWork->Manifest = Manifest;
			InWaiting.OverlayWork->Registry = Registry;
			InWaiting.OverlayWork->Resources = Resources;
			InWaiting.OverlayWork->Consumer = InWaiting.Consumer;
		}
		const auto Work = InWaiting.OverlayWork;
		uint64 RequiredInput = Work->MetadataLease->Bytes();
		int32 RequiredCount = 1;
		const auto SourceLease = Work->Read.GenerationLease ? Work->Read.GenerationLease->GetResourceLease() : nullptr;
		if (SourceLease)
		{
			if (!SourceLease->IsOwnedBy(Resources.Get())) return RejectGrant(EVoxelTerrainResourceAdmission::InvalidRequest);
			++RequiredCount;
			if (SourceLease->GetPool() == EVoxelTerrainResourcePool::Input)
			{
				if (SourceLease->Bytes() > MAX_uint64 - RequiredInput) return RejectGrant(EVoxelTerrainResourceAdmission::CapacityUnachievable);
				RequiredInput += SourceLease->Bytes();
			}
		}
		Request.SetRequiredBindingInput(RequiredInput, RequiredCount);
		Request.InputBytes = Work->MetadataLease->Bytes();
		Request.ReservedBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPersistentSection>();
		if (!Request.bBlockBound)
		{
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			if (!Request.BindGetBlockReason(Resources, InWaiting.Consumer,
				[Work]
				{
					if (!Work->bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
					uint64 Version = 0;
					return Work->Resources->TryGetReleaseVersionExcludingOwner(Work->Consumer, Version) &&
						Version > Work->CapacityVersion.Load() ? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
				}, Error))
				return DeferExactAdmission(InSection, InKey, InWaiting, Resources, BeforeAdmission, Error);
		}
		if (!Request.bExecuteBound)
		{
			BeforeAdmission = Resources->ReleaseVersionExcludingOwner(InWaiting.Consumer);
			if (!Request.BindExecute(Resources, InWaiting.Consumer,
				[Work](const TAtomic<bool>& InCancel) { return Work->Advance(InCancel); }, Error))
				return DeferExactAdmission(InSection, InKey, InWaiting, Resources, BeforeAdmission, Error);
		}
	}
	else return RejectGrant(EVoxelTerrainResourceAdmission::InvalidRequest);

	const FVoxelSectionStamp ExpectedStamp = InWaiting.Stamp;
	const FVoxelGenerationConsumerKey Consumer = InWaiting.Consumer;
	const EVoxelTaskKind Kind = Request.Kind;
	FVoxelTaskRequest Submitted = MoveTemp(Request);
	InWaiting.PendingRequest.Reset();
	InWaiting.bTaskSubmitted = true;
	++EnqueueAttempts;
	BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const bool bEnqueued = Scheduler.Enqueue(MoveTemp(Submitted));
	EnqueuedTasks += bEnqueued ? 1 : 0;
	FWaitingPlanSection* Current = WaitingPlanSections.Find(InKey);
	if (!Current || Current->Stamp != ExpectedStamp || Current->Consumer != Consumer ||
		!Runtime.IsCurrentStamp(InKey, ExpectedStamp)) return bEnqueued;
	if (bEnqueued)
	{
		Current->BaseWork.Reset();
		Current->OverlayWork.Reset();
		Current->AdmissionResources.Reset();
		if (Kind == EVoxelTaskKind::GenerateExactBase) PlanCoordinator->ReleaseConsumer(Consumer);
		return true;
	}
	Current->bTaskSubmitted = false;
	Current->PendingRequest.Emplace(MoveTemp(Submitted));
	FVoxelSection* Section = Runtime.FindSection(InKey);
	if (!Section) return false;
	const EVoxelTaskBlockReason Reason = Current->PendingRequest->Blocked;
	if (Reason == EVoxelTaskBlockReason::Oversized || Reason == EVoxelTaskBlockReason::TerminalFailure)
	{
		return DeferExactAdmission(*Section, InKey, *Current, Resources, BeforeAdmission,
			Reason == EVoxelTaskBlockReason::Oversized ? TEXT("CapacityUnachievable: exact task admission") :
			TEXT("InvalidRequest: exact task admission"));
	}
	if (Reason == EVoxelTaskBlockReason::InputMemory || Reason == EVoxelTaskBlockReason::Memory)
	{
		Current->AdmissionResources = Resources;
		Current->AdmissionCapacityVersion = BeforeAdmission;
		Current->bWaitingTaskCapacity = true;
	}
	return false;
}

bool FVoxelEmergeManager::RequestBase(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	const FVoxelExactDemand Demand = InDemand;
	FVoxelTaskStamp TaskStamp;
	TaskStamp.WorldEpoch =
		InSection.Stamp.Epoch;
	TaskStamp.Token =
		InSection.Stamp.Token;
	TaskStamp.Section =
		InKey;
	TaskStamp.OwnerId = GenerationOwnerId;
	TaskStamp.OwnerIncarnation = InSection.Stamp.Token;

	FWaitingPlanSection* Waiting = FindOrCreateWaiting(InSection, InKey);
	if (!Waiting) return false;
	FVoxelSection* CurrentSection = Runtime.FindSection(InKey);
	if (!CurrentSection || CurrentSection->Stamp != Waiting->Stamp) return false;
	TaskStamp.Revision = Waiting->Consumer.RequestSerial;
	if (Waiting->bTaskSubmitted) return false;
	if (Scheduler.Has(TaskStamp, EVoxelTaskKind::GenerateExactBase)) return false;
	if (Waiting->bWaitingTaskCapacity && Waiting->AdmissionResources->ReleaseVersionExcludingOwner(Waiting->Consumer) <=
		Waiting->AdmissionCapacityVersion) return false;
	Waiting->bWaitingTaskCapacity = false;
	if (Waiting->PendingRequest) return SubmitExactRequest(*Runtime.FindSection(InKey), InKey, *Waiting, Demand);
	const auto FactResources = Scheduler.GetRuntimeResources();
	if (Waiting->bWaitingCapacity && FactResources->ReleaseVersionExcludingOwner(Waiting->Consumer) <= Waiting->CapacityVersion) return false;
	Waiting->bWaitingCapacity = false;
	const uint64 Version = FactResources->ReleaseVersionExcludingOwner(Waiting->Consumer);
	if (!Waiting->Dependencies.Lease)
	{
		const FIntVector Min = InKey * 16;
		FString Error;
		if (!PlanCoordinator->GatherForBounds({ Min, Min + FIntVector(16) }, Waiting->Consumer, Waiting->Dependencies, Error))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked")))
			{
				Waiting->bWaitingCapacity = true;
				Waiting->CapacityVersion = Version;
				return false;
			}
			CurrentSection->Status = EVoxelSectionStatus::Failed;
			CurrentSection->Error = MoveTemp(Error);
			WaitingPlanSections.Remove(InKey);
			UE_LOG(LogTemp, Error, TEXT("Voxel plan preflight failed: section=%s error=%s"),
				*InKey.ToString(), *CurrentSection->Error);
			return false;
		}
	}
	if (!Waiting->Retention)
	{
		const FIntVector Min = InKey * 16;
		FString Error;
		if (!PlanCoordinator->RetainDependencies(Waiting->Dependencies.Values, FIntPoint(Min.X + 8, Min.Y + 8),
			Waiting->Consumer, Waiting->Retention, Error))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked")))
			{
				Waiting->bWaitingCapacity = true;
				Waiting->CapacityVersion = Version;
				return false;
			}
			CurrentSection->Status = EVoxelSectionStatus::Failed;
			CurrentSection->Error = MoveTemp(Error);
			WaitingPlanSections.Remove(InKey);
			return false;
		}
	}
	const double Now = FPlatformTime::Seconds();
	if (!IsCriticalDemand(Demand) && Now < Waiting->RetryAfterSeconds)
	{
		return false;
	}
	const FVoxelGenerationDependencyStatus PlanStatus = PlanCoordinator->Ensure(
		Waiting->Dependencies.Values,
		ResolveWorkClass(Demand), Demand.ServicePriority, Waiting->Consumer);
	if (PlanStatus.bFailed)
	{
		CurrentSection->Status = EVoxelSectionStatus::Failed;
		CurrentSection->Error = PlanStatus.Error;
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		UE_LOG(LogTemp, Error, TEXT("Voxel plan dependency failed: section=%s error=%s"),
			*InKey.ToString(), *CurrentSection->Error);
		return false;
	}
	const FVoxelOverlayPreflightStatus OverlayStatus =
		Generator->EnsureOverlayConsumerReady(Waiting->Consumer, {InKey * 16, InKey * 16 + FIntVector(16)},
			ResolveWorkClass(Demand), Demand.ServicePriority);
	if (OverlayStatus.bFailed)
	{
		CurrentSection->Status = EVoxelSectionStatus::Failed;
		CurrentSection->Error = OverlayStatus.Error;
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		UE_LOG(LogTemp, Error,
			TEXT("Voxel overlay preflight failed: section=%s error=%s"),
			*InKey.ToString(), *CurrentSection->Error);
		return false;
	}
	Waiting->bPlanReady = PlanStatus.bReady;
	Waiting->bOverlayReady = OverlayStatus.bReady;
	PlanPendingAttempts += !PlanStatus.bReady;
	PlanReadyAttempts += PlanStatus.bReady;
	OverlayPendingAttempts += !OverlayStatus.bReady;
	OverlayReadyAttempts += OverlayStatus.bReady;
	if (!PlanStatus.bReady || !OverlayStatus.bReady)
	{
		Waiting->RetryAfterSeconds = Now + (IsCriticalDemand(Demand) ? 0.05 : 0.2);
		return false;
	}
	Waiting->PendingRequest.Emplace();
	FVoxelTaskRequest& Request = Waiting->PendingRequest.GetValue();
	Request.Kind = EVoxelTaskKind::GenerateExactBase;
	Request.Stamp = TaskStamp;
	Request.GenerationConsumer = Waiting->Consumer;
	Request.bResumable = true;
	return SubmitExactRequest(*Runtime.FindSection(InKey), InKey, *Waiting, Demand);
}

bool FVoxelEmergeManager::ResolveOverlay(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	const FVoxelExactDemand Demand = InDemand;
	if (InSection.Status !=
		EVoxelSectionStatus::BaseReady)
	{
		return false;
	}

	bool bKnownNatural = false;
	if (!Runtime.IsServer())
	{
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		if (!RemoteOverlayResolver || !RemoteOverlayResolver(InKey, Revision, Overlay)) return false;
		FString Error;
		const TMap<int32, FVoxelBlockState> EmptyBlocks;
		const TMap<int32, FVoxelBlockEntityState> EmptyEntities;
		return Runtime.PublishFinal(InKey, Revision, Overlay ? Overlay->Blocks : EmptyBlocks,
			Overlay ? Overlay->Entities : EmptyEntities, Error);
	}

	// 已提交 Journal 也包含恢复自然值的 Tombstone；不能从稀疏磁盘索引推导版本为零。
	if (const auto Overlay = Runtime.FindCommittedOverlay(InKey))
	{
		FString Error;
		return Runtime.PublishFinal(InKey, Overlay->Revision, Overlay->Blocks, Overlay->Entities, Error);
	}
	if (!RegionStore.IsDirectoryReady()) return false;

	if (Runtime.IsServer())
	{
		bKnownNatural =
			!Runtime.GetChangeIndex().
				IsModified(InKey);
	}

	if (bKnownNatural)
	{
		FString Error;

		return Runtime.PublishFinal(
			InKey,
			0,
			{},
			{},
			Error);
	}

	if (!Runtime.IsServer())
	{
		return false;
	}

	FVoxelTaskStamp TaskStamp;
	TaskStamp.WorldEpoch = InSection.Stamp.Epoch;
	TaskStamp.Token = InSection.Stamp.Token;
	TaskStamp.Section = InKey;
	TaskStamp.OwnerId = GenerationOwnerId;
	TaskStamp.OwnerIncarnation = InSection.Stamp.Token;
	FWaitingPlanSection* Waiting = FindOrCreateWaiting(InSection, InKey);
	if (!Waiting) return false;
	TaskStamp.Revision = Waiting->Consumer.RequestSerial;
	if (Waiting->bTaskSubmitted) return false;
	if (Scheduler.Has(TaskStamp, EVoxelTaskKind::DecodeOverlay)) return false;
	if (Waiting->bWaitingTaskCapacity && Waiting->AdmissionResources->ReleaseVersionExcludingOwner(Waiting->Consumer) <=
		Waiting->AdmissionCapacityVersion) return false;
	Waiting->bWaitingTaskCapacity = false;
	if (!Waiting->PendingRequest)
	{
		Waiting->PendingRequest.Emplace();
		FVoxelTaskRequest& Request = Waiting->PendingRequest.GetValue();
		Request.Kind = EVoxelTaskKind::DecodeOverlay;
		Request.Stamp = TaskStamp;
		Request.GenerationConsumer = Waiting->Consumer;
		Request.bResumable = true;
	}
	return SubmitExactRequest(*Runtime.FindSection(InKey), InKey, *Waiting, Demand);
}

uint64 FVoxelEmergeManager::GetWaitingPlanConsumers(const EVoxelGenerationPlanKind InKind) const
{
	return PlanCoordinator->GetWaitingConsumers(InKind);
}

bool FVoxelEmergeManager::IsCriticalDemand(const FVoxelExactDemand& InDemand)
{
	return InDemand.bWarmupData || InDemand.bWarmupCollision || InDemand.bMovementCriticalData || InDemand.bMovementCriticalFine ||
		   InDemand.bMovementCriticalCollision;
}

FVoxelEmergeManager::~FVoxelEmergeManager()
{
	Generator->SetOverlayPlanningServices({});
	Generator->SetTypedOverlayConsumerWakeCallback({});
	Generator->SetOverlayServicePriorityDonation({});
	Generator->SetOverlayConsumerWakeCallback({});
	Reset();
}

void FVoxelEmergeManager::OnGenerationFactsReady(const FVoxelGenerationConsumerKey& InConsumer)
{
	if (InConsumer.WorldEpoch != Runtime.Epoch()) return;
	if (InConsumer.Kind == EVoxelGenerationConsumerKind::WorldFact)
	{
		Generator->NotifyOverlayPlanningDependencyReady(InConsumer);
		return;
	}
	if (ExternalFactConsumers.Contains(InConsumer))
	{
		if (ExternalConsumerWakeCallback) ExternalConsumerWakeCallback(InConsumer);
		return;
	}
	if (InConsumer.Kind != EVoxelGenerationConsumerKind::F0Section || InConsumer.OwnerId != GenerationOwnerId) return;
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InConsumer.Min);
	if (Waiting && Waiting->Consumer == InConsumer && Runtime.IsCurrentStamp(InConsumer.Min, Waiting->Stamp))
	{
		Waiting->RetryAfterSeconds = 0.0;
		WakeCriticalSection(InConsumer.Min);
	}
}

void FVoxelEmergeManager::WakeCriticalSection(const FIntVector& InKey)
{
	const FVoxelExactDemand* Demand = CurrentDemand.Find(InKey);
	if (!Demand || !IsCriticalDemand(*Demand))
	{
		return;
	}
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InKey);
	if (Waiting && !Runtime.IsCurrentStamp(InKey, Waiting->Stamp))
	{
		return;
	}
	if (Waiting)
	{
		Waiting->RetryAfterSeconds = 0.0;
	}
	if (!QueuedCriticalSections.Contains(InKey))
	{
		const bool bEnqueued = ReadyCriticalSections.Enqueue(InKey);
		if (!bEnqueued)
		{
			DeferTickAdmission(Scheduler.GetTerrainResources()->ReleaseVersionExcludingOwner(GetAdmissionConsumer()),
				TEXT("InvalidRequest: critical ready queue exceeds admitted demand capacity"));
			return;
		}
		QueuedCriticalSections.Add(InKey);
	}
}
