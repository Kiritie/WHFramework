#include "Voxel/Navigation/VoxelNavigationPresenter.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Voxel/Navigation/VoxelNavigationBounds.h"
#include "Voxel/Navigation/VoxelNavigationData.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

namespace
{
	struct FVoxelNavigationTaskPayload : FVoxelTaskCustomPayload
	{
		TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> Surface;
		FVoxelNavigationInputVersion Version;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;

		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationTaskPayload>() +
				(Surface ? VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationSurface>() +
					(Surface->Access ? VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationSurfaceAccess>() : 0) +
					Surface->Vertices.GetAllocatedSize() + Surface->Indices.GetAllocatedSize() : 0);
		}
		virtual uint64 GetResourceAccountedBytes() const override
		{
			return (MetadataLease ? MetadataLease->Bytes() : 0) +
				(Surface && Surface->MetadataLease ? Surface->MetadataLease->Bytes() : 0) +
				(Surface && Surface->ResourceLease ? Surface->ResourceLease->Bytes() : 0);
		}
	};

}

struct FVoxelNavigationWorker
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelSectionSnapshot Section;
	FVoxelSectionSnapshot Above;
	FVoxelSectionSnapshot SecondAbove;
	FVoxelNavigationInputVersion Version;
	TSharedPtr<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe> Consumers;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	uint64 BlockedReleaseVersion = 0;
	bool bCapacityBlocked = false;

	EVoxelTaskBlockReason BlockReason() const
	{
		return bCapacityBlocked && Resources->ReleaseVersionExcludingOwner(Consumer) <= BlockedReleaseVersion ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}

	FVoxelTaskResult Execute(const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		Result.InputResourceLease = InputLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PayloadLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SurfaceLease;
		TSharedPtr<FVoxelNavigationTaskPayload, ESPMode::ThreadSafe> Payload;
		TSharedPtr<FVoxelNavigationSurface, ESPMode::ThreadSafe> Surface;
		bCapacityBlocked = false;
		if (InCancel.Load()) return Result;
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* MetadataLeases[] = {&InputLease, &PayloadLease, &SurfaceLease};
		FVoxelGenerationRequiredSourceScope MetadataSources(Resources.Get(), MetadataLeases,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto Leases = static_cast<const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* const*>(Context);
				for (int32 Index = 0; Index < 3; ++Index)
					if (*Leases[Index]) FVoxelGenerationRequiredSourceScope::VisitLease(Leases[Index]->Get(), Visit);
			});
		const auto GrantMetadata = [&](const EVoxelTerrainResourcePool Pool, const uint64 Bytes,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
		{
			FVoxelGenerationRequiredCapacity Required;
			if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Result.Error)) return false;
			const auto Admission = Resources->TryAcquire(Consumer, Pool, Bytes, Lease, 0,
				Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
			if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
			Result.Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: navigation result metadata") :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: navigation result metadata and original inputs") :
				TEXT("InvalidRequest: navigation result metadata");
			return false;
		};
		if (GrantMetadata(EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationTaskPayload>(), PayloadLease) &&
			GrantMetadata(EVoxelTerrainResourcePool::Staged, VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationSurface>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationSurfaceAccess>(), SurfaceLease))
		{
			Payload = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationTaskPayload>(PayloadLease);
			Payload->MetadataLease = PayloadLease;
			Payload->Version = Version;
			Surface = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationSurface>(SurfaceLease);
			Surface->MetadataLease = SurfaceLease;
			Result.bSuccess = FVoxelNavigationSurfaceBuilder::Build(Section, Version.AboveToken ? &Above : nullptr,
				*Registry, *Shapes, Version.CellSize, Version.Clearance, *Surface, &InCancel,
				Resources, Consumer, &Result.Error, Version.SecondAboveToken ? &SecondAbove : nullptr);
			if (Result.bSuccess)
			{
				Surface->InputVersion = Version;
				Surface->Access = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationSurfaceAccess>(
					Surface->MetadataLease, Consumers.ToSharedRef());
				Payload->Surface = MoveTemp(Surface);
				Result.CustomPayload = MoveTemp(Payload);
			}
		}
		if (!Result.bSuccess && Result.Error.StartsWith(TEXT("CapacityBlocked")))
		{
			Payload.Reset();
			Surface.Reset();
			PayloadLease.Reset();
			SurfaceLease.Reset();
			BlockedReleaseVersion = BeforeAdmission;
			bCapacityBlocked = true;
			Result.bSuccess = true;
			Result.bContinue = true;
		}
		return Result;
	}
};

FVoxelNavigationPresenter::FVoxelNavigationPresenter(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InModule.GetWorldEpoch())
{
}

FVoxelNavigationPresenter::~FVoxelNavigationPresenter()
{
	Reset();
}

bool FVoxelNavigationPresenter::ReserveStorage(const int32 InSectionCount, const int32 InBoundsCount)
{
	if (bStopped || InSectionCount < 0 || InBoundsCount < 0)
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	if (StorageLease && InSectionCount <= SectionCapacity && InBoundsCount <= BoundsCapacity) return true;
	const int64 SectionRows = FMath::Max<int64>(16, InSectionCount > SectionCapacity ?
		FMath::Max<int64>(InSectionCount, int64(SectionCapacity) * 2) : SectionCapacity);
	const int64 BoundsRows = FMath::Max<int64>(8, InBoundsCount > BoundsCapacity ?
		FMath::Max<int64>(InBoundsCount, int64(BoundsCapacity) * 2) : BoundsCapacity);
	if (SectionRows > MAX_int32 || BoundsRows > MAX_int32)
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	const int32 Sections = int32(SectionRows);
	const int32 Bounds = int32(BoundsRows);
	const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(Wanted) + VoxelGenerationAllocation::AllocatedBytes(InputLeases) + VoxelGenerationAllocation::AllocatedBytes(PendingPromotion) +
		VoxelGenerationAllocation::AllocatedBytes(PendingRetirement) +
		VoxelGenerationAllocation::AllocatedBytes(Dirty) + VoxelGenerationAllocation::AllocatedBytes(PendingSections) +
		VoxelGenerationAllocation::AllocatedBytes(PublishedVersions) + VoxelGenerationAllocation::AllocatedBytes(Requested) + VoxelGenerationAllocation::AllocatedBytes(Prepared) +
		VoxelGenerationAllocation::AllocatedBytes(Admissions) + VoxelGenerationAllocation::AllocatedBytes(Failures) +
		VoxelGenerationAllocation::AllocatedBytes(Components) + VoxelGenerationAllocation::AllocatedBytes(RegionBounds);
	const uint64 NewBytes = VoxelGenerationAllocation::Private::SetReserveBytes<FIntVector>(Wanted, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FInputLeases>>(InputLeases, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FIntVector>(PendingPromotion, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FIntVector>(PendingRetirement, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, int32>>(Dirty, Sections) +
		VoxelGenerationAllocation::Private::SparseArrayReserveBytes<FIntVector>(PendingSections, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FVoxelNavigationInputVersion>>(PublishedVersions, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FVoxelTaskStamp>>(Requested, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe>>>(Prepared, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FAdmission>>(Admissions, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FVoxelNavigationInputVersion>>(Failures, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, TObjectPtr<UVoxelNavigationSurfaceComponent>>>(Components, Sections) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FBoundsLease>>(RegionBounds, Bounds);
	const uint64 Metadata = sizeof(*this) + VoxelBoundedAllocation::WorkerSharedBytes<TAtomic<bool>>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, 1, {}, 1);
	const auto Resources = Scheduler.GetTerrainResources();
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), StorageLease.Get(), Required, Error))
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	const uint64 RequiredInput = Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
	ActivationAdmission = StorageLease ? StorageLease->TryResize(Metadata + OldBytes + NewBytes, RequiredInput, Required.LeaseCount) :
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Metadata + OldBytes + NewBytes,
			StorageLease, 0, RequiredInput, Required.LeaseCount);
	if (ActivationAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	Wanted.Reserve(Sections);
	InputLeases.Reserve(Sections);
	PendingPromotion.Reserve(Sections);
	PendingRetirement.Reserve(Sections);
	Dirty.Reserve(Sections);
	PendingSections.Reserve(Sections);
	PublishedVersions.Reserve(Sections);
	Requested.Reserve(Sections);
	Prepared.Reserve(Sections);
	Admissions.Reserve(Sections);
	Failures.Reserve(Sections);
	Components.Reserve(Sections);
	RegionBounds.Reserve(Bounds);
	SectionCapacity = Sections;
	BoundsCapacity = Bounds;
	if (!Lifetime) Lifetime = VoxelBoundedAllocation::MakeWorkerShared<TAtomic<bool>>(StorageLease, true);
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Wanted) + VoxelGenerationAllocation::AllocatedBytes(InputLeases) + VoxelGenerationAllocation::AllocatedBytes(PendingPromotion) +
		VoxelGenerationAllocation::AllocatedBytes(PendingRetirement) +
		VoxelGenerationAllocation::AllocatedBytes(Dirty) + VoxelGenerationAllocation::AllocatedBytes(PendingSections) +
		VoxelGenerationAllocation::AllocatedBytes(PublishedVersions) + VoxelGenerationAllocation::AllocatedBytes(Requested) + VoxelGenerationAllocation::AllocatedBytes(Prepared) +
		VoxelGenerationAllocation::AllocatedBytes(Admissions) + VoxelGenerationAllocation::AllocatedBytes(Failures) +
		VoxelGenerationAllocation::AllocatedBytes(Components) + VoxelGenerationAllocation::AllocatedBytes(RegionBounds);
	check(ActualBytes <= NewBytes);
	const bool bResized = StorageLease->Resize(Metadata + ActualBytes);
	check(bResized);
	return true;
}

FIntVector FVoxelNavigationPresenter::BoundsGroup(const FIntVector& InSection)
{
	return FIntVector(VoxelGeneration::FloorDivide(InSection.X, 2),
		VoxelGeneration::FloorDivide(InSection.Y, 2), VoxelGeneration::FloorDivide(InSection.Z, 2));
}

bool FVoxelNavigationPresenter::CreateBounds(const FIntVector& InGroup, TWeakObjectPtr<AVoxelNavigationBounds>& OutBounds)
{
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	if (bStopped || !Module.IsAuthority() || !Navigation) return false;
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	auto* Bounds = Module.GetWorld()->SpawnActor<AVoxelNavigationBounds>(Parameters);
	if (!Bounds) return false;
	// 每个固定块最多归组 2^3 个 Section；只扩展固定 halo，不累积移动轨迹的大 AABB。
	const double Side = 32.0 * Module.BlockSize();
	const FVector Minimum = FVector(InGroup) * Side;
	const FBox Box(Minimum - FVector(Module.BlockSize()),
		Minimum + FVector(Side) + FVector(Module.BlockSize(), Module.BlockSize(), Module.NavigationClearanceCentimeters));
	Bounds->SetBounds(Box);
	const auto* Data = Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance());
	const double TileSide = Data ? Data->GetTileSizeUU() : GetDefault<AVoxelNavigationData>()->TileSizeUU;
	const double Radius = FMath::Sqrt(2.0) * Side * 0.5 + TileSide;
	Navigation->RegisterNavigationInvoker(Bounds, Radius, Radius + TileSide);
	OutBounds = Bounds;
	return true;
}

bool FVoxelNavigationPresenter::AcquireBounds(const FIntVector& InSection)
{
	const FIntVector Group = BoundsGroup(InSection);
	FBoundsLease* Lease = RegionBounds.Find(Group);
	if (!Lease)
	{
		TWeakObjectPtr<AVoxelNavigationBounds> Bounds;
		if (!CreateBounds(Group, Bounds)) return false;
		Lease = &RegionBounds.Add(Group, {Bounds, 0});
	}
	else if (!Lease->Bounds.IsValid() && !CreateBounds(Group, Lease->Bounds)) return false;
	check(Lease->References < MAX_int32);
	++Lease->References;
	return true;
}

void FVoxelNavigationPresenter::ReleaseBounds(const FIntVector& InSection)
{
	const FIntVector Group = BoundsGroup(InSection);
	auto* Lease = RegionBounds.Find(Group);
	check(Lease && Lease->References > 0);
	if (--Lease->References != 0) return;
	if (auto* Bounds = Lease->Bounds.Get())
	{
		if (auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld()))
			Navigation->UnregisterNavigationInvoker(Bounds);
		Bounds->Destroy();
	}
	RegionBounds.Remove(Group);
}

bool FVoxelNavigationPresenter::ActivateSection(const FIntVector& InSection)
{
	ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityBlocked;
	if (bStopped || WorldEpoch != Module.GetWorldEpoch())
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	if (Wanted.Contains(InSection))
	{
		InputLeases.FindChecked(InSection).Consumers->SetAdmissionOpen(true);
		PendingRetirement.Remove(InSection);
		ActivationAdmission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	if (Wanted.Num() == MAX_int32 || RegionBounds.Num() == MAX_int32)
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	if (!ReserveStorage(Wanted.Num() + 1, RegionBounds.Num() + (RegionBounds.Contains(BoundsGroup(InSection)) ? 0 : 1))) return false;
	FInputLeases Leases;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ConsumerLease;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, 1, InSection, 1);
	const auto Resources = Scheduler.GetTerrainResources();
	FVoxelGenerationRequiredSourceScope StorageSource(&Resources.Get(), StorageLease.Get());
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, Error))
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	ActivationAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Resident,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationSectionConsumers>(), ConsumerLease, 0,
		Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (ActivationAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	Leases.Consumers = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationSectionConsumers>(MoveTemp(ConsumerLease));
	if (InSection.Z > MAX_int32 - 2 || !FMath::IsFinite(Module.NavigationClearanceCentimeters) ||
		Module.NavigationClearanceCentimeters <= 0.0 || Module.NavigationClearanceCentimeters > 32.0 * Module.BlockSize())
	{
		ActivationAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	ActivationAdmission = EVoxelTerrainResourceAdmission::CapacityBlocked;
	if (!Module.AcquireNavigationDataSection(InSection, Leases.Section)) return false;
	if (!Module.AcquireNavigationDataSection(InSection + FIntVector(0, 0, 1), Leases.Above))
	{
		Module.ReleaseNavigationDataSection(InSection, Leases.Section);
		return false;
	}
	if (Module.NavigationClearanceCentimeters > 16.0 * Module.BlockSize() &&
		!Module.AcquireNavigationDataSection(InSection + FIntVector(0, 0, 2), Leases.SecondAbove))
	{
		Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 1), Leases.Above);
		Module.ReleaseNavigationDataSection(InSection, Leases.Section);
		return false;
	}
	if (!AcquireBounds(InSection))
	{
		if (Leases.SecondAbove.ConsumerId.IsValid()) Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 2), Leases.SecondAbove);
		Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 1), Leases.Above);
		Module.ReleaseNavigationDataSection(InSection, Leases.Section);
		return false;
	}
	if (!Module.EnsureNavigationCollisionSection(InSection))
	{
		ReleaseBounds(InSection);
		if (Leases.SecondAbove.ConsumerId.IsValid()) Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 2), Leases.SecondAbove);
		Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 1), Leases.Above);
		Module.ReleaseNavigationDataSection(InSection, Leases.Section);
		return false;
	}
	InputLeases.Add(InSection, Leases);
	Wanted.Add(InSection);
	InvalidateSection(InSection);
	ActivationAdmission = EVoxelTerrainResourceAdmission::Granted;
	return true;
}

void FVoxelNavigationPresenter::RetireSection(const FIntVector& InSection)
{
	if (bStopped) return;
	auto* Leases = InputLeases.Find(InSection);
	if (!Leases) return;
	Leases->Consumers->SetAdmissionOpen(false);
	if (Leases->Consumers->Num() != 0)
	{
		if (!PendingRetirement.Contains(InSection)) PendingRetirement.Add(InSection);
		return;
	}
	RetireResources(InSection);
}

void FVoxelNavigationPresenter::RetireResources(const FIntVector& InSection)
{
	PendingRetirement.Remove(InSection);
	if (Wanted.Remove(InSection) != 0) ReleaseBounds(InSection);
	if (const int32* PendingIndex = Dirty.Find(InSection)) PendingSections.RemoveAt(*PendingIndex);
	Dirty.Remove(InSection);
	PublishedVersions.Remove(InSection);
	Requested.Remove(InSection);
	Prepared.Remove(InSection);
	Admissions.Remove(InSection);
	Failures.Remove(InSection);
	PendingPromotion.Remove(InSection);
	Scheduler.CancelMatching([this, InSection](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildNavigation && Stamp.OwnerId == OwnerId &&
			Stamp.WorldEpoch == WorldEpoch && Stamp.Section == InSection;
	});
	if (TObjectPtr<UVoxelNavigationSurfaceComponent>* Component = Components.Find(InSection))
	{
		if (*Component)
		{
			(*Component)->Apply(nullptr);
			(*Component)->DestroyComponent();
		}
		Components.Remove(InSection);
	}
	FInputLeases Leases;
	if (InputLeases.RemoveAndCopyValue(InSection, Leases))
	{
		if (Leases.SecondAbove.ConsumerId.IsValid()) Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 2), Leases.SecondAbove);
		Module.ReleaseNavigationDataSection(InSection + FIntVector(0, 0, 1), Leases.Above);
		Module.ReleaseNavigationDataSection(InSection, Leases.Section);
	}
}

void FVoxelNavigationPresenter::InvalidateSection(const FIntVector& InSection)
{
	if (bStopped) return;
	for (const FIntVector& Section : {InSection, InSection - FIntVector(0, 0, 1), InSection - FIntVector(0, 0, 2)})
	{
		if (Section == InSection - FIntVector(0, 0, 2))
		{
			const auto* Leases = InputLeases.Find(Section);
			if (!Leases || !Leases->SecondAbove.ConsumerId.IsValid()) continue;
		}
		if (Wanted.Contains(Section))
		{
			MarkDirty(Section);
			PublishedVersions.Remove(Section);
			Requested.Remove(Section);
			Prepared.Remove(Section);
			Admissions.Remove(Section);
			Failures.Remove(Section);
			PendingPromotion.Remove(Section);
			Scheduler.CancelMatching([this, Section](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return Kind == EVoxelTaskKind::BuildNavigation && Stamp.OwnerId == OwnerId &&
					Stamp.WorldEpoch == WorldEpoch && Stamp.Section == Section;
			});
			if (const auto* Component = Components.Find(Section); Component && *Component)
			{
				(*Component)->Apply(nullptr);
			}
		}
	}
}

void FVoxelNavigationPresenter::Tick()
{
	if (bStopped) return;
	if (const auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld()))
	{
		if (auto* Data = Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance()))
		{
			Data->RetirePathInputs(Module.GetTerrainFrameContext());
			Data->RetryBlockedTiles(Module.GetTerrainFrameContext());
			Data->AdvancePathMaintenance(Module.GetTerrainFrameContext());
		}
	}
	const double Deadline = Module.GetTerrainFrameContext().Deadline(0.25);
	for (int32 Step = 0; Step < 32 && !PendingRetirement.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		auto& Frame = Module.GetTerrainFrameContext();
		if (Frame.IsActive() && !Frame.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission,
			Started, RetirementCostMilliseconds)) break;
		RetirementCursor %= PendingRetirement.GetMaxIndex();
		const auto Id = FSetElementId::FromInteger(RetirementCursor++);
		if (PendingRetirement.IsValidId(Id))
		{
			const FIntVector Section = PendingRetirement.Get(Id);
			const auto* Leases = InputLeases.Find(Section);
			if (!Leases || Leases->Consumers->Num() == 0) RetireResources(Section);
		}
		const double Finished = FPlatformTime::Seconds();
		RetirementCostMilliseconds = FMath::Lerp(RetirementCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (Frame.IsActive()) Frame.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
	for (int32 Step = 0; Step < 32 && !PendingPromotion.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		auto& Frame = Module.GetTerrainFrameContext();
		if (Frame.IsActive() && !Frame.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
			Started, PromotionCostMilliseconds)) break;
		PromotionCursor %= PendingPromotion.GetMaxIndex();
		const auto Id = FSetElementId::FromInteger(PromotionCursor++);
		if (PendingPromotion.IsValidId(Id))
		{
			const auto* Component = Components.Find(PendingPromotion.Get(Id));
			if (!Component || !*Component) PendingPromotion.Remove(Id);
			else
			{
				const auto& Surface = (*Component)->GetSurface();
				const bool bMetadata = !Surface.MetadataLease || Surface.MetadataLease->Transfer(EVoxelTerrainResourcePool::Resident);
				const bool bGeometry = !Surface.ResourceLease || Surface.ResourceLease->Transfer(EVoxelTerrainResourcePool::Resident);
				if (bMetadata && bGeometry) PendingPromotion.Remove(Id);
			}
		}
		const double Finished = FPlatformTime::Seconds();
		PromotionCostMilliseconds = FMath::Lerp(PromotionCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (Frame.IsActive()) Frame.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
	}
	int32 Remaining = 4;
	int32 Attempts = FMath::Min(64, PendingSections.GetMaxIndex());
	while (Remaining > 0 && Attempts-- > 0 && !PendingSections.IsEmpty() && FPlatformTime::Seconds() < Deadline)
	{
		DirtyCursor %= PendingSections.GetMaxIndex();
		const int32 Index = DirtyCursor++;
		if (!PendingSections.IsValidIndex(Index)) continue;
		const FIntVector Section = PendingSections[Index];
		const double Started = FPlatformTime::Seconds();
		auto& Frame = Module.GetTerrainFrameContext();
		if (Frame.IsActive() && !Frame.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
			Started, PrepareCostMilliseconds)) break;
		if (Prepared.Contains(Section) ? ApplyPrepared(Section) : RequestSection(Section))
		{
			Dirty.Remove(Section);
			PendingSections.RemoveAt(Index);
			--Remaining;
		}
		const double Finished = FPlatformTime::Seconds();
		PrepareCostMilliseconds = FMath::Lerp(PrepareCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (Frame.IsActive()) Frame.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
	}
}

void FVoxelNavigationPresenter::MarkDirty(const FIntVector& InSection)
{
	if (!Dirty.Contains(InSection))
	{
		Dirty.Add(InSection, PendingSections.Add(InSection));
	}
}

bool FVoxelNavigationPresenter::RequestSection(const FIntVector& InSection)
{
	FVoxelNavigationInputVersion Version;
	if (bStopped || Requested.Contains(InSection) || !Module.IsCollisionReady(InSection) ||
		!CaptureVersion(InSection, Version))
	{
		return false;
	}
	if (const auto* Failed = Failures.Find(InSection); Failed && *Failed == Version) return false;
	FAdmission& Admission = Admissions.FindOrAdd(InSection);
	if (Admission.Stamp.Token == 0 || Admission.Version != Version)
	{
		const auto PreviousStamp = Admission.Stamp;
		if (PreviousStamp.Token) Scheduler.CancelMatching([PreviousStamp](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildNavigation && Stamp == PreviousStamp;
		});
		Admission.Request.Reset();
		Admission.Worker.Reset();
		Admission.bCapacityBlocked = false;
		Admission.Version = Version;
		Admission.FirstRunnableSeconds = FPlatformTime::Seconds();
		Admission.Stamp.OwnerId = OwnerId;
		Admission.Stamp.OwnerIncarnation = Version.SectionIncarnation;
		Admission.Stamp.WorldEpoch = WorldEpoch;
		check(NextRequestSerial < MAX_uint64);
		Admission.Stamp.Token = ++NextRequestSerial;
		Admission.Stamp.Revision = Version.SectionRevision;
		Admission.Stamp.GeometryRevision = Version.LocalGeometryRevision;
		Admission.Stamp.RegistryRevision = Version.RegistryRevision;
		Admission.Stamp.ShapeRevision = Version.ShapeRevision;
		Admission.Stamp.Section = InSection;
	}
	const FVoxelTaskStamp Stamp = Admission.Stamp;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId,
		Version.SectionIncarnation, InSection, Stamp.Token);
	const auto Resources = Scheduler.GetTerrainResources();
	if (Admission.bCapacityBlocked)
	{
		uint64 Current = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(Consumer, Current) || Current <= Admission.CapacityVersion) return false;
		Admission.bCapacityBlocked = false;
	}
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!Admission.Worker)
	{
		const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationWorker>() +
			(Version.SecondAboveToken ? 3 : (Version.AboveToken ? 2 : 1)) * (VoxelBoundedAllocation::InitialGrowBytes<uint32>(4096) +
				6 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(256));
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, InputBytes, InputLease,
			0, StorageLease ? StorageLease->Bytes() : 0, int32(StorageLease.IsValid()) + 1);
		if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (InputAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				Admission.bCapacityBlocked = true;
				Admission.CapacityVersion = BeforeAdmission;
			}
			else Failures.FindOrAdd(InSection) = Version;
			return false;
		}
		Admission.Worker = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationWorker>(InputLease);
		const auto Worker = Admission.Worker;
		Worker->InputLease = MoveTemp(InputLease);
		Worker->Resources = Resources;
		Worker->Consumer = Consumer;
		Worker->Version = Version;
		Worker->Consumers = InputLeases.FindChecked(InSection).Consumers;
		Worker->Registry = Module.GetRegistry();
		Worker->Shapes = Module.GetShapes();
		if (!Module.GetRuntime()->CaptureSnapshot(InSection, Worker->Section, Worker->InputLease) ||
			(Version.AboveToken && !Module.GetRuntime()->CaptureSnapshot(InSection + FIntVector(0, 0, 1), Worker->Above, Worker->InputLease)) ||
			(Version.SecondAboveToken && !Module.GetRuntime()->CaptureSnapshot(InSection + FIntVector(0, 0, 2), Worker->SecondAbove, Worker->InputLease)))
		{
			Admission.Worker.Reset();
			return false;
		}
		check(Worker->Section.Stamp.Token == Version.SectionToken && Worker->Section.Revision == Version.SectionRevision);
		check(!Version.AboveToken || (Worker->Above.Stamp.Token == Version.AboveToken && Worker->Above.Revision == Version.AboveRevision));
		check(!Version.SecondAboveToken || (Worker->SecondAbove.Stamp.Token == Version.SecondAboveToken &&
			Worker->SecondAbove.Revision == Version.SecondAboveRevision));
		Admission.Request.Emplace();
		auto& Request = Admission.Request.GetValue();
		Request.Kind = EVoxelTaskKind::BuildNavigation;
		Request.WorkClass = EVoxelWorkClass::Interactive;
		Request.Stamp = Stamp;
		Request.GenerationConsumer = Consumer;
		Request.SetRequiredBindingInput(Worker->InputLease->Bytes() + StorageLease->Bytes(), 3);
		if (const auto* Demand = Module.GetCurrentInterest().Exact.Find(InSection))
		{
			Request.ServiceBand = Demand->ServicePriority.Band;
			Request.SourcePriority = Demand->ServicePriority.SourcePriority;
			Request.SourceStableKey = Demand->ServicePriority.SourceStableKey;
			Request.DistanceScore = Demand->ServicePriority.DistanceCells;
			Request.ForwardScore = Demand->ServicePriority.ForwardScore;
		}
		Request.ReservedBytes = 1;
		Request.FirstRunnableSeconds = Admission.FirstRunnableSeconds;
		Request.bResumable = true;
	}
	const auto Worker = Admission.Worker;
	auto& Request = Admission.Request.GetValue();
	auto GetBlockReason = [Worker]()
	{
		return Worker->BlockReason();
	};
	auto Execute = [Worker](const TAtomic<bool>& InCancel)
	{
		return Worker->Execute(InCancel);
	};
	auto ApplyCallback = [this, Gate = Lifetime](FVoxelTaskResult&& InResult)
	{
		if (Gate->Load()) this->Apply(MoveTemp(InResult));
	};
	FString BindingError;
	if ((!Request.bBlockBound && !Request.BindGetBlockReason(Resources, Consumer, MoveTemp(GetBlockReason), BindingError)) ||
		(!Request.bExecuteBound && !Request.BindExecute(Resources, Consumer, MoveTemp(Execute), BindingError)) ||
		(!Request.bApplyBound && !Request.BindApply(Resources, Consumer, MoveTemp(ApplyCallback), BindingError)))
	{
		if (BindingError.StartsWith(TEXT("CapacityBlocked")))
		{
			Admission.bCapacityBlocked = true;
			Admission.CapacityVersion = BeforeAdmission;
		}
		else
		{
			Failures.FindOrAdd(InSection) = Version;
			Admission.Request.Reset();
			Admission.Worker.Reset();
		}
		return false;
	}
	FVoxelTaskRequest AdmissionRequest = MoveTemp(Request);
	Admission.Request.Reset();
	const bool bAccepted = Scheduler.Enqueue(MoveTemp(AdmissionRequest));
	FAdmission* Current = Admissions.Find(InSection);
	if (!Current || Current->Stamp != Stamp || Current->Worker != Worker || bStopped) return false;
	if (!bAccepted)
	{
		Current->Request.Emplace(MoveTemp(AdmissionRequest));
		if (Current->Request->Blocked == EVoxelTaskBlockReason::Oversized || Current->Request->Blocked == EVoxelTaskBlockReason::TerminalFailure)
		{
			Failures.FindOrAdd(InSection) = Version;
			Current->Request.Reset();
			Current->Worker.Reset();
		}
		else if (Current->Request->Blocked == EVoxelTaskBlockReason::InputMemory || Current->Request->Blocked == EVoxelTaskBlockReason::Memory)
		{
			Current->bCapacityBlocked = true;
			Current->CapacityVersion = BeforeAdmission;
		}
		return false;
	}
	Current->Worker.Reset();
	Requested.FindOrAdd(InSection) = Stamp;
	return true;
}

bool FVoxelNavigationPresenter::CaptureVersion(const FIntVector& InSection, FVoxelNavigationInputVersion& OutVersion) const
{
	const auto Runtime = Module.GetRuntime();
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	const auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
	const auto* Leases = InputLeases.Find(InSection);
	const auto* Above = Runtime ? Runtime->FindSection(InSection + FIntVector(0, 0, 1)) : nullptr;
	if (!Leases || !Section || Section->Status != EVoxelSectionStatus::DataReady || !Registry || !Shapes ||
		Section->GetResourceIdentity() != Leases->Section.Resource || !Above || Above->Status != EVoxelSectionStatus::DataReady ||
		Above->GetResourceIdentity() != Leases->Above.Resource) return false;
	FVoxelPhysicsReceipt Physics;
	if (!Module.CaptureCollisionReceipt(InSection, Physics)) return false;
	OutVersion = {};
	OutVersion.WorldEpoch = WorldEpoch;
	OutVersion.Section = InSection;
	OutVersion.SectionIncarnation = Section->Incarnation;
	OutVersion.SectionToken = Section->Stamp.Token;
	OutVersion.SectionRevision = Section->CommittedRevision;
	OutVersion.LocalGeometryRevision = Runtime->GetChangeHierarchy().GetSectionRevision(InSection);
	OutVersion.AboveIncarnation = Above->Incarnation;
	OutVersion.AboveToken = Above->Stamp.Token;
	OutVersion.AboveRevision = Above->CommittedRevision;
	OutVersion.AboveGeometryRevision = Runtime->GetChangeHierarchy().GetSectionRevision(InSection + FIntVector(0, 0, 1));
	if (Module.NavigationClearanceCentimeters > 16.0 * Module.BlockSize())
	{
		const auto* SecondAbove = Runtime->FindSection(InSection + FIntVector(0, 0, 2));
		if (!SecondAbove || SecondAbove->Status != EVoxelSectionStatus::DataReady ||
			SecondAbove->GetResourceIdentity() != Leases->SecondAbove.Resource) return false;
		OutVersion.SecondAboveIncarnation = SecondAbove->Incarnation;
		OutVersion.SecondAboveToken = SecondAbove->Stamp.Token;
		OutVersion.SecondAboveRevision = SecondAbove->CommittedRevision;
		OutVersion.SecondAboveGeometryRevision = Runtime->GetChangeHierarchy().GetSectionRevision(InSection + FIntVector(0, 0, 2));
	}
	OutVersion.RegistryRevision = Registry->Hash;
	OutVersion.ShapeRevision = Shapes->GetContentHash();
	OutVersion.PhysicsSceneEpoch = Physics.SceneEpoch;
	OutVersion.PhysicsOwnerId = Physics.PresenterOwnerId;
	OutVersion.CollisionBodyGeneration = Physics.BodyGeneration;
	OutVersion.CellSize = Module.BlockSize();
	OutVersion.Clearance = Module.NavigationClearanceCentimeters;
	return OutVersion.MatchesPhysicsReceipt(Physics);
}

void FVoxelNavigationPresenter::Apply(FVoxelTaskResult&& InResult)
{
	const auto* RequestedStamp = Requested.Find(InResult.Stamp.Section);
	if (bStopped || InResult.Stamp.OwnerId != OwnerId || InResult.Stamp.WorldEpoch != WorldEpoch ||
		!Wanted.Contains(InResult.Stamp.Section) || !RequestedStamp || *RequestedStamp != InResult.Stamp)
	{
		return;
	}
	const auto* Admission = Admissions.Find(InResult.Stamp.Section);
	const FVoxelNavigationInputVersion AttemptVersion = Admission ? Admission->Version : FVoxelNavigationInputVersion();
	Requested.Remove(InResult.Stamp.Section);
	Admissions.Remove(InResult.Stamp.Section);
	FVoxelNavigationInputVersion Version;
	const bool bHasVersion = CaptureVersion(InResult.Stamp.Section, Version);
	const auto Payload = StaticCastSharedPtr<const FVoxelNavigationTaskPayload>(InResult.CustomPayload);
	if (!InResult.bSuccess || InResult.bCanceled || !Payload || !Payload->Surface ||
		!bHasVersion || Payload->Version != Version || Payload->Surface->InputVersion != Payload->Version)
	{
		if (!InResult.bSuccess && !InResult.bCanceled && bHasVersion && AttemptVersion == Version)
		{
			Failures.Add(InResult.Stamp.Section, Version);
		}
		MarkDirty(InResult.Stamp.Section);
		return;
	}
	Payload->WorkReceipt = InResult.WorkReceipt;
	Prepared.FindOrAdd(InResult.Stamp.Section) = Payload;
	if (!ApplyPrepared(InResult.Stamp.Section)) MarkDirty(InResult.Stamp.Section);
}

bool FVoxelNavigationPresenter::ApplyPrepared(const FIntVector& InSection)
{
	const auto* PreparedPayload = Prepared.Find(InSection);
	if (!PreparedPayload) return false;
	const auto Payload = StaticCastSharedPtr<const FVoxelNavigationTaskPayload>(*PreparedPayload);
	FVoxelNavigationInputVersion Version;
	if (!Wanted.Contains(InSection) || !CaptureVersion(InSection, Version) || Payload->Version != Version ||
		!Payload->Surface || Payload->Surface->InputVersion != Payload->Version)
	{
		Prepared.Remove(InSection);
		return false;
	}
	const auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	if (auto* Data = Navigation ? Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance()) : nullptr;
		Data && !Data->BindTerrain(Module, Scheduler.GetTerrainResources())) return false;
	const auto* Previous = Components.Find(InSection);
	const bool bReplacing = Previous && *Previous;
	const auto Transfer = [bReplacing](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		return Lease->Transfer(EVoxelTerrainResourcePool::Resident) ||
			(bReplacing && Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap));
	};
	if (!Transfer(Payload->Surface->MetadataLease) || !Transfer(Payload->Surface->ResourceLease)) return false;
	AActor* Owner = Module.GetTypedOuter<AActor>();
	if (!Owner)
	{
		return false;
	}
	TObjectPtr<UVoxelNavigationSurfaceComponent>& Component = Components.FindOrAdd(InSection);
	if (!Component)
	{
		Component = NewObject<UVoxelNavigationSurfaceComponent>(Owner);
		Component->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Component->RegisterComponent();
	}
	Component->SetWorldLocation(FVector(InSection * 16) * Module.BlockSize());
	Payload->Surface->WorkReceipt = Payload->WorkReceipt;
	Component->Apply(Payload->Surface);
	PublishedVersions.FindOrAdd(InSection) = Version;
	if (!PendingPromotion.Contains(InSection)) PendingPromotion.Add(InSection);
	Prepared.Remove(InSection);
	return true;
}

bool FVoxelNavigationPresenter::IsReady(const FIntVector& InSection) const
{
	FVoxelNavigationInputVersion Version;
	const auto* Published = PublishedVersions.Find(InSection);
	if (bStopped || !Published || !CaptureVersion(InSection, Version) || *Published != Version || Dirty.Contains(InSection))
	{
		return false;
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	FNavLocation Projected;
	const auto* Component = Components.Find(InSection);
	if (!Component || !*Component || (*Component)->GetSurface().Indices.IsEmpty()) return false;
	const FBox Bounds = (*Component)->GetSurface().Bounds.TransformBy((*Component)->GetComponentTransform());
	if (!Bounds.IsValid) return false;
	const FVector Center = Bounds.GetCenter();
	const auto* Data = Navigation ? Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance()) : nullptr;
	return Data && Navigation->ProjectPointToNavigation(Center, Projected, FVector(8.0 * Module.BlockSize()), Data) &&
		Bounds.ExpandBy(Module.BlockSize()).IsInsideOrOn(Projected.Location) &&
		Data->HasCurrentTileForNode(Projected.NodeRef, Version);
}

bool FVoxelNavigationPresenter::IsInputCurrent(const FVoxelNavigationInputVersion& InVersion) const
{
	FVoxelNavigationInputVersion Current;
	const auto* Published = PublishedVersions.Find(InVersion.Section);
	return !bStopped && Published && *Published == InVersion && !Dirty.Contains(InVersion.Section) &&
		Wanted.Contains(InVersion.Section) && CaptureVersion(InVersion.Section, Current) && Current == InVersion;
}

bool FVoxelNavigationPresenter::IsResident(const FIntVector& InSection) const
{
	return !bStopped && Wanted.Contains(InSection);
}

bool FVoxelNavigationPresenter::HasReaders() const
{
	for (const auto& Pair : InputLeases)
	{
		if (Pair.Value.Consumers->Num() != 0) return true;
	}
	return false;
}

bool FVoxelNavigationPresenter::TryCloseForWorldStop()
{
	check(IsInGameThread());
	// 先阻止每个 Section 新的有效读取，再检查现存读取者，避免检查后才取得引用。
	for (const auto& Pair : InputLeases) Pair.Value.Consumers->SetAdmissionOpen(false);
	const auto* System = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	auto* Data = System ? Cast<AVoxelNavigationData>(System->GetDefaultNavDataInstance()) : nullptr;
	const bool bNativeClosed = !Data || Data->TryCloseQueriesForWorldStop();
	if (bNativeClosed && !HasReaders()) return true;
	if (Data && bNativeClosed) Data->ResumeQueriesAfterWorldStopWait();
	for (const auto& Pair : InputLeases)
	{
		if (!PendingRetirement.Contains(Pair.Key)) Pair.Value.Consumers->SetAdmissionOpen(true);
	}
	return false;
}

void FVoxelNavigationPresenter::Reset()
{
	if (bStopped) return;
	bStopped = true;
	if (const auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld()))
	{
		if (auto* Data = Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance())) Data->UnbindTerrain(Module);
	}
	if (Lifetime) Lifetime->Store(false);
	for (const auto& Pair : InputLeases) Pair.Value.Consumers->CloseWorld();
	Scheduler.CancelMatching([this](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildNavigation && Stamp.OwnerId == OwnerId && Stamp.WorldEpoch == WorldEpoch;
	});
	for (auto& Pair : Components)
	{
		if (Pair.Value)
		{
			Pair.Value->Apply(nullptr);
			Pair.Value->DestroyComponent();
		}
	}
	for (auto& Pair : RegionBounds)
	{
		if (AVoxelNavigationBounds* Bounds = Pair.Value.Bounds.Get())
		{
			if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld()))
			{
				Navigation->UnregisterNavigationInvoker(Bounds);
			}
			Bounds->Destroy();
		}
	}
	Components.Empty();
	RegionBounds.Empty();
	for (const auto& Pair : InputLeases)
	{
		if (Pair.Value.SecondAbove.ConsumerId.IsValid()) Module.ReleaseNavigationDataSection(Pair.Key + FIntVector(0, 0, 2), Pair.Value.SecondAbove);
		Module.ReleaseNavigationDataSection(Pair.Key + FIntVector(0, 0, 1), Pair.Value.Above);
		Module.ReleaseNavigationDataSection(Pair.Key, Pair.Value.Section);
	}
	InputLeases.Empty();
	PendingPromotion.Empty();
	PendingRetirement.Empty();
	PublishedVersions.Empty();
	Requested.Empty();
	Prepared.Empty();
	Admissions.Empty();
	Failures.Empty();
	Dirty.Empty();
	PendingSections.Empty();
	Wanted.Empty();
	Lifetime.Reset();
	StorageLease.Reset();
}
