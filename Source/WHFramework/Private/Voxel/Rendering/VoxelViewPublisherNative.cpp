#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	bool NativePublisherHasExternalRelease(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 Version)
	{
		uint64 Current = 0;
		return Resources->TryGetReleaseVersionExcludingOwner(Consumer, Current) && Current > Version;
	}

	bool NativePublisherIsTerminalAdmission(const EVoxelTaskBlockReason Reason)
	{
		return (static_cast<uint16>(Reason) & (static_cast<uint16>(EVoxelTaskBlockReason::Oversized) |
			static_cast<uint16>(EVoxelTaskBlockReason::TerminalFailure))) != 0;
	}

	struct FNativeMeshReceipt
	{
		struct FInput
		{
			int32 UpdateIndex = INDEX_NONE;
			FVoxelTaskStamp Stamp;
			FVector Origin = FVector::ZeroVector;
			double Scale = 1.0;
			bool bAllowResidentOverlap = false;
		};
		TArray<FInput> Inputs;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	};

	struct FNativeMeshBatch final : FVoxelTaskCustomPayload
	{
		TSharedPtr<FNativeMeshReceipt, ESPMode::ThreadSafe> Receipt;
		TArray<TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>> Sources;
		TArray<TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe>> Meshes;
		virtual uint64 GetAllocatedBytes() const override
		{
			uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshBatch>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshReceipt>() + Receipt->Inputs.GetAllocatedSize() +
				Sources.GetAllocatedSize() + Meshes.GetAllocatedSize();
			for (const auto& Mesh : Meshes) if (Mesh) Bytes += Mesh->GetAllocatedBytes();
			return Bytes;
		}
		virtual uint64 GetResourceAccountedBytes() const override
		{
			uint64 Bytes = Receipt->Lease->Bytes();
			for (const auto& Mesh : Meshes) if (Mesh) Bytes += Mesh->GetResourceAccountedBytes();
			return Bytes;
		}
	};

	struct FNativeMeshWorker
	{
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FNativeMeshBatch, ESPMode::ThreadSafe> Batch;
		TOptional<FVoxelTerrainCoarseMeshBuilder> Builder;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
		int32 SourceIndex = 0;
		uint64 SourceTriangles = 0;
		uint64 SliceCount = 0;
		double NextDiagnosticSeconds = 0.0;

		FNativeMeshWorker(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			const FVoxelGenerationConsumerKey& InConsumer, TSharedPtr<FNativeMeshBatch, ESPMode::ThreadSafe> InBatch)
			: Resources(InResources), Consumer(InConsumer), Batch(MoveTemp(InBatch))
		{
		}

		static void VisitInputs(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Worker = *static_cast<const FNativeMeshWorker*>(InContext);
			const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
			{
				if (Lease && Lease->IsOwnedBy(Worker.Resources.Get()))
					FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
			};
			Visit(Worker.InputLease);
			for (const auto& Source : Worker.Batch->Sources) Visit(Source->ResourceLease);
		}

		static void VisitSources(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			VisitInputs(InContext, InVisit);
			const auto& Worker = *static_cast<const FNativeMeshWorker*>(InContext);
			for (const auto& Mesh : Worker.Batch->Meshes)
				if (Mesh && Mesh->MetadataLease && Mesh->MetadataLease->IsOwnedBy(Worker.Resources.Get()))
					FVoxelGenerationRequiredSourceScope::VisitLease(Mesh->MetadataLease.Get(), InVisit);
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PublisherNativeBatchAdvance);
			FVoxelTaskResult Result;
			bWaitingCapacity.Store(false);
			const FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this, &VisitSources);
			const uint64 BeforeBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			const double Started = FPlatformTime::Seconds();
			const double Deadline = Started + 0.002;
			EVoxelTerrainCoarseMeshProgress Progress = EVoxelTerrainCoarseMeshProgress::Working;
			int32 Microsteps = 0;
			for (; Microsteps < 64 && FPlatformTime::Seconds() < Deadline; ++Microsteps)
			{
				if (InCancel.Load())
				{
					Progress = EVoxelTerrainCoarseMeshProgress::Canceled;
					break;
				}
				const auto& Input = Batch->Receipt->Inputs[SourceIndex];
				if (!Builder.IsSet())
				{
					Builder.Emplace(Resources, Consumer, true, Input.bAllowResidentOverlap);
					if (!Builder->Initialize(Batch->Sources[SourceIndex], Input.Origin, Input.Scale, Input.Stamp, Result.Error))
					{
						Result.Error = TEXT("InvalidRequest: native coarse batch: ") + Result.Error;
						Progress = EVoxelTerrainCoarseMeshProgress::Failed;
						break;
					}
				}
				Progress = Builder->Advance(128, InCancel, Result.Error);
				if (Progress == EVoxelTerrainCoarseMeshProgress::Complete)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PublisherNativeBatchSourceComplete);
					Batch->Meshes[SourceIndex] = Builder->TakeResult();
					if (!Batch->Meshes[SourceIndex])
					{
						Result.Error = TEXT("InvalidRequest: native coarse batch result");
						Progress = EVoxelTerrainCoarseMeshProgress::Failed;
						break;
					}
					Builder.Reset();
					if (++SourceIndex == Batch->Sources.Num()) break;
					Progress = EVoxelTerrainCoarseMeshProgress::Working;
				}
				else if (Progress != EVoxelTerrainCoarseMeshProgress::Working) break;
			}
			++SliceCount;
#if !UE_BUILD_SHIPPING
			const double Finished = FPlatformTime::Seconds();
			if (Finished >= NextDiagnosticSeconds || SourceIndex == Batch->Sources.Num())
			{
				const auto& Key = Batch->Receipt->Inputs[0].Stamp.GeometryIdentity.Key;
				UE_LOG(LogTemp, Display, TEXT("Native coarse batch slice: publication=%llu representation=%d level=%d coordinate=(%d,%d,%d) sources=%d/%d triangles=%llu slice=%llu microsteps=%d elapsedMs=%.3f progress=%d"),
					Consumer.OwnerIncarnation, int32(Key.Representation), Key.Level, Key.Coordinate.X, Key.Coordinate.Y, Key.Coordinate.Z,
					SourceIndex, Batch->Sources.Num(), SourceTriangles, SliceCount, Microsteps, (Finished - Started) * 1000.0, int32(Progress));
				NextDiagnosticSeconds = Finished + 10.0;
			}
#endif
			if (SourceIndex == Batch->Sources.Num())
			{
				Result.CustomPayload = Batch;
				Result.bSuccess = true;
			}
			else if (Progress == EVoxelTerrainCoarseMeshProgress::Working || Progress == EVoxelTerrainCoarseMeshProgress::CapacityBlocked)
			{
				Result.bSuccess = Result.bContinue = true;
				Result.bContinueImmediately = Progress == EVoxelTerrainCoarseMeshProgress::Working;
				if (Progress == EVoxelTerrainCoarseMeshProgress::CapacityBlocked)
				{
					CapacityVersion.Store(BeforeBuild);
					bWaitingCapacity.Store(true);
				}
			}
			else Result.bCanceled = Progress == EVoxelTerrainCoarseMeshProgress::Canceled;
			return Result;
		}
	};

}

bool FVoxelViewPublisher::IsNativeGeometryCurrent(const FVoxelTaskStamp& InStamp) const
{
	if (InStamp.WorldEpoch != WorldEpoch || !InStamp.OwnerId.IsValid() || InStamp.OwnerIncarnation == 0 ||
		InStamp.Token == 0 || !InStamp.bHasGeometryIdentity) return false;
	if (InStamp.OwnerId != NativeOwnerId && (!GeometryValidator || !GeometryValidator(InStamp))) return false;
	const auto* Geometry = Module.GetCurrentInterest().Geometry.Find(InStamp.GeometryIdentity.Key);
	if (!Geometry || *Geometry != InStamp.GeometryIdentity) return false;
	const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	const auto& Key = Geometry->Key;
	const uint64 Revision = Key.Representation == EVoxelTerrainRepresentation::VoxelProxy ?
		Hierarchy.GetVoxelProxyRevision({Key.Coordinate, Key.Level}) : Key.Representation == EVoxelTerrainRepresentation::Surface ?
		Hierarchy.GetSurfaceRevision({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}) :
		Hierarchy.GetMacroRevision({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
	return Revision == InStamp.Revision;
}

bool FVoxelViewPublisher::IsCurrentAtomEvidence(const FIntVector& InSection, const FVoxelFineOwnerEvidence& InEvidence) const
{
	(void)InSection;
	if (InEvidence.Content.WorldEpoch != WorldEpoch || InEvidence.Content.RecipeRevision != Module.GetManifest().RecipeHash ||
		InEvidence.Content.RegistryRevision != Module.GetRegistry()->Hash || InEvidence.Content.ShapeRevision != Module.GetShapes()->GetContentHash() ||
		(InEvidence.Owner.Kind != EVoxelFineOwnerKind::Proxy && InEvidence.Owner.Kind != EVoxelFineOwnerKind::Surface &&
		 InEvidence.Owner.Kind != EVoxelFineOwnerKind::Macro)) return false;
	const auto Geometry = StaticCastSharedPtr<const FVoxelTerrainCoarseMesh>(InEvidence.GeometryLease);
	return Geometry && IsNativeGeometryCurrent(Geometry->Stamp) && Geometry->Source &&
		(!MeshValidator || MeshValidator(*Geometry->Source));
}

FVoxelViewPublisher::ENativeUpdateWork FVoxelViewPublisher::GetNativeUpdateWork(const FPublication& InPublication, const FUpdate& InUpdate) const
{
	const FEntry* Entry = Entries.Find(InUpdate.SourceId);
	if (!Entry) return ENativeUpdateWork::None;
	const bool* Hidden = InPublication.Visibility.Find(InUpdate.SourceId);
	// 未声明显隐变化时继承当前状态；保留源正文变化不能复活已隐藏/退休几何。
	const bool bHidden = Entry->bRetiring || (Hidden ? *Hidden : Entry->bHidden);
	if (bHidden)
	{
		return Entry->bPresented && !Entry->bHidden ? ENativeUpdateWork::Remove : ENativeUpdateWork::None;
	}
	if (Hidden)
	{
		const bool bPresentedMatches = Entry->bPresented && !Entry->bHidden && Entry->NativeMesh &&
			Entry->PresentedSource == InUpdate.Source && Entry->PresentedNativeStamp == InUpdate.NativeStamp &&
			Entry->NativeMesh->Source == InUpdate.Source && Entry->NativeMesh->Stamp == InUpdate.NativeStamp &&
			Entry->NativeMesh->WorldCellOrigin == InUpdate.Location / Module.BlockSize() &&
			Entry->NativeMesh->CellScale == InUpdate.Scale / Module.BlockSize();
		if (!bPresentedMatches) return ENativeUpdateWork::Upload;
	}
	return !InUpdate.bUnchanged && InUpdate.bSourceDirty ? ENativeUpdateWork::Upload : ENativeUpdateWork::None;
}

bool FVoxelViewPublisher::HasNativeGroupWork(const FPublication& InPublication, const FGroup& InGroup) const
{
	if (InGroup.bPresentationCommitted) return false;
	return InGroup.NativeTransaction.IsValid() || InGroup.NativeCoarse || !InGroup.NativeChanges.IsEmpty() ||
		InGroup.UpdateIndices.ContainsByPredicate([this, &InPublication](const int32 Index)
	{
		return GetNativeUpdateWork(InPublication, InPublication.Updates[Index]) != ENativeUpdateWork::None;
	});
}

bool FVoxelViewPublisher::PrepareUpdate(const FPublicationRef& InPublication, const int32 InIndex,
	const double InDeadline, int32& InOutPreparedComponents, bool& bOutComplete)
{
	(void)InOutPreparedComponents;
	FUpdate& Update = InPublication->Updates[InIndex];
	if (Update.Source && Update.Source->Stamp.bHasGeometryIdentity && Update.Source->Stamp != Update.NativeStamp) return false;
	if (!Update.bSourceDirty && !Update.NativeMesh)
		if (const auto* Entry = Entries.Find(Update.SourceId); Entry && Entry->NativeMesh &&
			Entry->PresentedSource == Update.Source && Entry->PresentedNativeStamp == Update.NativeStamp &&
			Entry->NativeMesh->WorldCellOrigin == Update.Location / Module.BlockSize() &&
			Entry->NativeMesh->CellScale == Update.Scale / Module.BlockSize()) Update.NativeMesh = Entry->NativeMesh;
	if (Update.NativeMesh && (Update.NativeMesh->Source != Update.Source || Update.NativeMesh->Stamp != Update.NativeStamp ||
		Update.NativeMesh->WorldCellOrigin != Update.Location / Module.BlockSize() ||
		Update.NativeMesh->CellScale != Update.Scale / Module.BlockSize())) Update.NativeMesh.Reset();
	bOutComplete = GetNativeUpdateWork(InPublication.Get(), Update) != ENativeUpdateWork::Upload || Update.NativeMesh.IsValid();
	if (bOutComplete || Update.bNativeMeshPending || FPlatformTime::Seconds() >= InDeadline) return true;
	// 只等待会生产同一冻结网格的前代；无索引工作的控制更新不是生产者。
	if (Update.bRetainSource && !Update.bSourceDirty)
	{
		const FPublication* PreviousPublication = nullptr;
		const FUpdate* Previous = FindProjectedUpdate(Update.SourceId, InPublication->Serial, &PreviousPublication);
		if (Previous && Previous->Source == Update.Source && Previous->NativeStamp == Update.NativeStamp &&
			Previous->Location == Update.Location && Previous->Scale == Update.Scale)
		{
			if (Previous->NativeMesh && Previous->NativeMesh->Source == Update.Source &&
				Previous->NativeMesh->Stamp == Update.NativeStamp &&
				Previous->NativeMesh->WorldCellOrigin == Update.Location / Module.BlockSize() &&
				Previous->NativeMesh->CellScale == Update.Scale / Module.BlockSize())
			{
				Update.NativeMesh = Previous->NativeMesh;
				bOutComplete = true;
				return true;
			}
			if (PreviousPublication && GetNativeUpdateWork(*PreviousPublication, *Previous) == ENativeUpdateWork::Upload) return true;
		}
	}
	if (!Update.Source || !IsNativeGeometryCurrent(Update.NativeStamp) ||
		(Update.Source->Stamp.bHasGeometryIdentity && Update.Source->Stamp != Update.NativeStamp) ||
		(MeshValidator && !MeshValidator(*Update.Source))) return false;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId,
		InPublication->Serial, FIntVector(MIN_int32, 7, InIndex), Update.NativeStamp.Token);
	if (Update.bNativeAdmissionWaiting && !NativePublisherHasExternalRelease(Resources, Consumer, Update.NativeAdmissionVersion)) return true;
	Update.bNativeAdmissionWaiting = false;
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const auto RejectAdmission = [&](const bool bTerminal, const EVoxelTaskBlockReason Reason)
	{
		FVoxelTaskStamp Stamp = Update.NativeStamp;
		Stamp.OwnerId = NativeOwnerId;
		Stamp.OwnerIncarnation = InPublication->Serial;
		Stamp.Section = FIntVector(MIN_int32, 7, InIndex);
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildViewCoverage, Stamp, Reason);
		if (bTerminal) DiscardPublication(InPublication, false);
		else
		{
			Update.NativeAdmissionVersion = BeforeAdmission;
			Update.bNativeAdmissionWaiting = true;
		}
	};
	FIntVector OwnershipMin, OwnershipMax;
	if (!Update.NativeStamp.GeometryIdentity.Key.TryBounds(Update.NativeStamp.GeometryIdentity.WorldMinZ,
		Update.NativeStamp.GeometryIdentity.WorldMaxZ, OwnershipMin, OwnershipMax))
	{
		RejectAdmission(true, EVoxelTaskBlockReason::TerminalFailure);
		return true;
	}
	const auto& Group = InPublication->Groups[Update.GroupIndex];
	check(Group.UpdateIndices[Group.PreparedUpdates] == InIndex);
	const int32 MaximumWorkers = FMath::Max(1, Module.GetTerrainFrameContext().GetAllocation().MaximumWorkers);
	const int32 MaximumSources = FMath::DivideAndRoundUp(MaximumWorkers, FMath::Max(1, ActiveNativeGroups));
	int32 SourceCount = 1;
	for (; SourceCount < MaximumSources && Group.PreparedUpdates + SourceCount < Group.UpdateIndices.Num() &&
		FPlatformTime::Seconds() < InDeadline; ++SourceCount)
	{
		const int32 NextIndex = Group.UpdateIndices[Group.PreparedUpdates + SourceCount];
		const auto& Next = InPublication->Updates[NextIndex];
		FIntVector NextMin, NextMax;
		if (!Next.Source || !Next.Source->bValidated || !Next.bSourceDirty ||
			Next.bRetainSource || Next.NativeMesh || Next.bNativeMeshPending || Next.bNativeAdmissionWaiting ||
			!IsNativeGeometryCurrent(Next.NativeStamp) || GetNativeUpdateWork(InPublication.Get(), Next) != ENativeUpdateWork::Upload ||
			(Next.Source->Stamp.bHasGeometryIdentity && Next.Source->Stamp != Next.NativeStamp) ||
			(MeshValidator && !MeshValidator(*Next.Source)) ||
			!Next.NativeStamp.GeometryIdentity.Key.TryBounds(Next.NativeStamp.GeometryIdentity.WorldMinZ,
				Next.NativeStamp.GeometryIdentity.WorldMaxZ, NextMin, NextMax)) break;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE_CONDITIONAL(Voxel_PublisherNativeBatchAdmission, SourceCount > 1);
	uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshWorker>();
	for (const uint64 Bytes : {VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshReceipt>(),
		VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshBatch>(),
		VoxelBoundedAllocation::ArrayBytes<FNativeMeshReceipt::FInput>(SourceCount),
		VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>>(SourceCount),
		VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe>>(SourceCount)})
	{
		if (Bytes > MAX_uint64 - InputBytes)
		{
			RejectAdmission(true, EVoxelTaskBlockReason::Oversized);
			return true;
		}
		InputBytes += Bytes;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, InputBytes, InputLease);
	if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		const bool bTerminal = InputAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked;
		RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
		return true;
	}
	const auto Receipt = VoxelBoundedAllocation::MakeWorkerShared<FNativeMeshReceipt>(InputLease);
	Receipt->Lease = InputLease;
	Receipt->Inputs.Reserve(SourceCount);
	const auto Batch = VoxelBoundedAllocation::MakeWorkerShared<FNativeMeshBatch>(InputLease);
	Batch->Receipt = Receipt;
	Batch->Sources.Reserve(SourceCount);
	Batch->Meshes.Reserve(SourceCount);
	Batch->Meshes.SetNum(SourceCount);
	for (int32 Offset = 0; Offset < SourceCount; ++Offset)
	{
		const int32 Index = Group.UpdateIndices[Group.PreparedUpdates + Offset];
		const auto& Source = InPublication->Updates[Index];
		FIntVector Min, Max;
		verify(Source.NativeStamp.GeometryIdentity.Key.TryBounds(Source.NativeStamp.GeometryIdentity.WorldMinZ,
			Source.NativeStamp.GeometryIdentity.WorldMaxZ, Min, Max));
		Receipt->Inputs.Add({Index, Source.NativeStamp, Source.Location / Module.BlockSize(), Source.Scale / Module.BlockSize(),
			TerrainRenderer->HasPresentedCoverage(FBox(FVector(Min), FVector(Max)))});
		Batch->Sources.Add(Source.Source);
	}
	auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FNativeMeshWorker>(InputLease, Resources, Consumer, Batch);
	Worker->InputLease = MoveTemp(InputLease);
	for (const auto& Source : Batch->Sources)
		for (const auto& SourceBatch : Source->Batches) Worker->SourceTriangles += uint64(SourceBatch.Mesh.Triangles.Num() / 3);
	FVoxelGenerationRequiredCapacity Required;
	FString InputError;
	const FVoxelGenerationRequiredSourceScope Inputs(&Resources.Get(), Worker.Get(), &FNativeMeshWorker::VisitInputs);
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, InputError))
	{
		RejectAdmission(true, EVoxelTaskBlockReason::Oversized);
		return true;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.Stamp = Update.NativeStamp;
	Request.Stamp.OwnerId = NativeOwnerId;
	Request.Stamp.OwnerIncarnation = InPublication->Serial;
	Request.Stamp.Section = FIntVector(MIN_int32, 7, InIndex);
	Request.bResumable = true;
	Request.ReservedBytes = 1;
	Request.bPublicationContinuation = true;
	Request.TerrainStage = Update.TerrainStage;
	Request.WorkClass = EVoxelWorkClass::Visible;
	Request.InputBytes = Worker->InputLease->Bytes();
	Request.GenerationConsumer = Consumer;
	Request.SetRequiredBindingInput(Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	SetPublicationPriority(Request, *InPublication, MakeArrayView(&Update.GroupIndex, 1));
	{
		FString CallbackBindingError;
		if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Worker]
	{
		return Worker->bWaitingCapacity.Load() && !NativePublisherHasExternalRelease(Worker->Resources, Worker->Consumer, Worker->CapacityVersion.Load()) ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}, CallbackBindingError))
		{
			const bool bTerminal = !CallbackBindingError.StartsWith(TEXT("CapacityBlocked"));
			RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
			return true;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Worker](const TAtomic<bool>& InCancel) { return Worker->Advance(InCancel); }, CallbackBindingError))
		{
			const bool bTerminal = !CallbackBindingError.StartsWith(TEXT("CapacityBlocked"));
			RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
			return true;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[this, WeakPublication = TWeakPtr<FPublication>(InPublication), Receipt, GroupIndex = Update.GroupIndex](FVoxelTaskResult&& InResult)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PublisherNativeBatchApply);
		const auto PinnedPublication = WeakPublication.Pin();
		if (!PinnedPublication) return;
		const FPublicationRef Publication = PinnedPublication.ToSharedRef();
		if (Publication->bCanceled) return;
		bool bAllCurrent = InResult.Stamp.WorldEpoch == WorldEpoch;
		for (const auto& Input : Receipt->Inputs)
		{
			if (!Publication->Updates.IsValidIndex(Input.UpdateIndex)) { bAllCurrent = false; continue; }
			auto& Target = Publication->Updates[Input.UpdateIndex];
			if (Target.NativeStamp != Input.Stamp) { bAllCurrent = false; continue; }
			Target.bNativeMeshPending = false;
			bAllCurrent &= IsNativeGeometryCurrent(Input.Stamp);
		}
		if (InResult.bCanceled && bAllCurrent && !Scheduler.IsTerminal({InResult.Kind, InResult.Stamp}))
		{
			for (const auto& Input : Receipt->Inputs) Publication->Updates[Input.UpdateIndex].bNativeAdmissionWaiting = false;
			QueueReadyGroup(Publication, GroupIndex);
			return;
		}
		if (!InResult.bSuccess || InResult.bCanceled || !bAllCurrent)
		{
			const bool bTerminal = Scheduler.IsTerminal({InResult.Kind, InResult.Stamp}) ||
				InResult.Error.StartsWith(TEXT("Oversized")) || InResult.Error.StartsWith(TEXT("CapacityUnachievable")) ||
				InResult.Error.StartsWith(TEXT("InvalidRequest"));
			DiscardPublication(Publication, !bTerminal);
			if (!bTerminal) bRetry = true;
			return;
		}
		const auto Result = StaticCastSharedPtr<const FNativeMeshBatch>(InResult.CustomPayload);
		bool bValid = Result && Result->Receipt == Receipt && Result->Meshes.Num() == Receipt->Inputs.Num();
		for (int32 Index = 0; bValid && Index < Receipt->Inputs.Num(); ++Index)
		{
			const auto& Input = Receipt->Inputs[Index];
			const auto& Mesh = Result->Meshes[Index];
			bValid = Mesh && Mesh->Source == Publication->Updates[Input.UpdateIndex].Source &&
				Mesh->Stamp == Input.Stamp && Mesh->WorldCellOrigin == Input.Origin && Mesh->CellScale == Input.Scale;
		}
		if (!bValid)
		{
			DiscardPublication(Publication);
			bRetry = true;
			return;
		}
		for (int32 Index = 0; Index < Receipt->Inputs.Num(); ++Index)
		{
			const auto& Mesh = Result->Meshes[Index];
			Mesh->WorkReceipt = InResult.WorkReceipt;
			Publication->Updates[Receipt->Inputs[Index].UpdateIndex].NativeMesh = Mesh;
		}
		QueueReadyGroup(Publication, GroupIndex);
	}, CallbackBindingError))
		{
			const bool bTerminal = !CallbackBindingError.StartsWith(TEXT("CapacityBlocked"));
			RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
			return true;
		}
	}
	for (const auto& Input : Receipt->Inputs) InPublication->Updates[Input.UpdateIndex].bNativeMeshPending = true;
	if (!Scheduler.Enqueue(MoveTemp(Request)))
	{
		for (const auto& Input : Receipt->Inputs)
			if (InPublication->Updates[Input.UpdateIndex].NativeStamp == Input.Stamp)
				InPublication->Updates[Input.UpdateIndex].bNativeMeshPending = false;
		RejectAdmission(NativePublisherIsTerminalAdmission(Request.Blocked), Request.Blocked);
	}
	return true;
}

bool FVoxelViewPublisher::AdvanceNativeGroup(const FPublicationRef& InPublication, FGroup& InGroup, bool& bOutAdvanced)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PublisherNativeGroupAdvance);
	bOutAdvanced = false;
	// 无几何与可见性工作的控制组沿原提交门禁完成，不占用原生准备并发槽位。
	if (!HasNativeGroupWork(InPublication.Get(), InGroup)) return true;
	if (!InGroup.bNativeAdmitted)
	{
		if (!AdmitNativeGroup(InGroup)) return false;
		bOutAdvanced = true;
	}
	const auto NativeResources = Scheduler.GetTerrainResources();
	struct FNativeGroupSources
	{
		const FVoxelViewPublisher& Owner;
		const FPublication& Publication;
		const FGroup& Group;
		const FVoxelTerrainResources& Resources;

		static void Enumerate(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Context = *static_cast<const FNativeGroupSources*>(InContext);
			const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
			{
				if (Lease && Lease->IsOwnedBy(Context.Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
			};
			Context.Owner.TerrainRenderer->VisitNativeControlSources(InVisit);
			for (const int32 Index : Context.Group.UpdateIndices)
			{
				const auto& Update = Context.Publication.Updates[Index];
				if (Update.Source) FVoxelTerrainRenderer::VisitMeshSources(*Update.Source, Context.Resources, InVisit);
				if (Update.NativeMesh)
				{
					Visit(Update.NativeMesh->MetadataLease);
					if (Update.NativeMesh->Source) FVoxelTerrainRenderer::VisitMeshSources(*Update.NativeMesh->Source, Context.Resources, InVisit);
				}
				if (const auto* Entry = Context.Owner.Entries.Find(Update.SourceId); Entry && Entry->NativeMesh)
				{
					Visit(Entry->NativeMesh->MetadataLease);
					if (Entry->NativeMesh->Source) FVoxelTerrainRenderer::VisitMeshSources(*Entry->NativeMesh->Source, Context.Resources, InVisit);
				}
			}
			Visit(Context.Group.NativeGroupLease);
			if (Context.Group.NativeCoarse) FVoxelTerrainRenderer::VisitCoarseSources(*Context.Group.NativeCoarse, Context.Resources, InVisit);
			for (const auto& Change : Context.Group.NativeChanges)
				if (Change.Item) FVoxelTerrainRenderer::VisitCoarseSources(*Change.Item, Context.Resources, InVisit);
		}
	};
	const FNativeGroupSources SourceContext{*this, InPublication.Get(), InGroup, NativeResources.Get()};
	const FVoxelGenerationRequiredSourceScope SourceScope(&NativeResources.Get(), &SourceContext, &FNativeGroupSources::Enumerate);
	const auto MeasureSources = [&](FVoxelGenerationRequiredCapacity& OutRequired)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PublisherNativeGroupSources);
		FString Error;
		return FVoxelGenerationRequiredSourceScope::Collect(NativeResources.Get(), nullptr, OutRequired, Error);
	};
	if (InGroup.bNativeControlWaiting)
	{
		const bool bBootstrapReady = InGroup.bNativeBootstrapWaiting &&
			TerrainRenderer->GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::Granted;
		if (!bBootstrapReady && TerrainRenderer->GetCoarseAdmissionVersion() == InGroup.NativeFrontendVersion &&
			!NativePublisherHasExternalRelease(NativeResources, InGroup.NativeControlConsumer, InGroup.NativeControlVersion)) return false;
		InGroup.bNativeControlWaiting = false;
		bOutAdvanced = true;
	}
	const auto RejectNativeAdmission = [&](const EVoxelTerrainResourceAdmission Admission,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 BeforeAdmission, const bool bBootstrap)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			bOutAdvanced |= !InGroup.bNativeControlWaiting || InGroup.NativeControlConsumer != Consumer ||
				InGroup.NativeControlVersion != BeforeAdmission;
			InGroup.NativeControlConsumer = Consumer;
			InGroup.NativeControlVersion = BeforeAdmission;
			InGroup.NativeFrontendVersion = TerrainRenderer->GetCoarseAdmissionVersion();
			InGroup.bNativeControlWaiting = true;
			InGroup.bNativeBootstrapWaiting = bBootstrap;
		}
		else
		{
			DiscardPublication(InPublication, false);
			bOutAdvanced = true;
		}
	};
	if (InGroup.NativeTransaction.IsValid())
	{
		if (TerrainRenderer->ConsumeTransactionFailure(InGroup.NativeTransaction))
		{
			DiscardPublication(InPublication, false);
			bOutAdvanced = true;
			return false;
		}
		if (!InGroup.bNativeSubmitted)
		{
			if (TerrainRenderer->IsTransactionPending(InGroup.NativeTransaction)) return false;
			InGroup.NativeTransaction.Invalidate();
			bOutAdvanced = true;
			return false;
		}
		InGroup.NativeTransaction.Invalidate();
		return true;
	}
	if (!InGroup.NativeGroupLease && !InGroup.UpdateIndices.IsEmpty())
	{
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, InPublication->Serial,
			{}, InGroup.QueueSequence);
		const uint64 BeforeAdmission = NativeResources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredCapacity Required;
		if (!MeasureSources(Required))
		{
			RejectNativeAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable, Consumer, BeforeAdmission, false);
			return false;
		}
		const auto Admission = NativeResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainCoarseMutation>(InGroup.UpdateIndices.Num()), InGroup.NativeGroupLease,
			0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			RejectNativeAdmission(Admission, Consumer, BeforeAdmission, false);
			return false;
		}
		InGroup.NativeChanges.Reserve(InGroup.UpdateIndices.Num());
		check(InGroup.NativeChanges.GetAllocatedSize() <= InGroup.NativeGroupLease->Bytes());
		bOutAdvanced = true;
	}
	if (!InGroup.UpdateIndices.IsValidIndex(InGroup.NativeSourceCursor))
	{
		if (InGroup.NativeChanges.IsEmpty()) return true;
		const int32 GroupIndex = int32(&InGroup - InPublication->Groups.GetData());
		FGuid Transaction;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, InPublication->Serial,
			FIntVector(MIN_int32, 12, GroupIndex), InGroup.QueueSequence);
		const uint64 BeforeAdmission = NativeResources->ReleaseVersionExcludingOwner(Consumer);
		const auto MakeCurrent = [&](TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		{
			return [this, WeakPublication = TWeakPtr<FPublication>(InPublication), GroupIndex, Lease = MoveTemp(InLease)]
			{
				const auto Publication = WeakPublication.Pin();
				if (!Publication || Publication->bCanceled || !Publication->Groups.IsValidIndex(GroupIndex)) return false;
				for (const int32 Index : Publication->Groups[GroupIndex].UpdateIndices)
				{
					if (!Publication->Updates.IsValidIndex(Index)) return false;
					const auto& Update = Publication->Updates[Index];
					if (!Entries.Contains(Update.SourceId)) return false;
					const bool* Hidden = Publication->Visibility.Find(Update.SourceId);
					if ((!Hidden || !*Hidden) && (!IsNativeGeometryCurrent(Update.NativeStamp) ||
						!Update.Source || (MeshValidator && !MeshValidator(*Update.Source)))) return false;
				}
				return true;
			};
		};
		const auto MakeSubmitted = [&](TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		{
			return [WeakPublication = TWeakPtr<FPublication>(InPublication), GroupIndex, Lease = MoveTemp(InLease)]
			{
				const auto Publication = WeakPublication.Pin();
				if (Publication && !Publication->bCanceled && Publication->Groups.IsValidIndex(GroupIndex))
				{
					auto& Group = Publication->Groups[GroupIndex];
					Group.bNativeSubmitted = true;
					if (Group.NativeGroupLease) Group.NativeGroupLease->MarkPublished();
				}
			};
		};
		auto CurrentPrototype = MakeCurrent(nullptr);
		auto SubmittedPrototype = MakeSubmitted(nullptr);
		const uint64 CallbackBytes = VoxelBoundedAllocation::FunctionBytes<decltype(CurrentPrototype), false>() +
			VoxelBoundedAllocation::FunctionBytes<decltype(SubmittedPrototype), false>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CallbackLease;
		if (CallbackBytes)
		{
			FVoxelGenerationRequiredCapacity Required;
			if (!MeasureSources(Required))
			{
				RejectNativeAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable, Consumer, BeforeAdmission, false);
				return false;
			}
			const auto CallbackAdmission = NativeResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
				CallbackBytes, CallbackLease, 0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
			if (CallbackAdmission != EVoxelTerrainResourceAdmission::Granted)
			{
				RejectNativeAdmission(CallbackAdmission, Consumer, BeforeAdmission, false);
				return false;
			}
		}
		EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		if (TerrainRenderer->PrepareCoarseTransaction(InGroup.NativeChanges,
			TFunction<bool()>(MakeCurrent(CallbackLease)), TFunction<void()>(MakeSubmitted(CallbackLease)),
			Transaction, InGroup.Priority.Service, InGroup.FirstQueuedAt, &Admission, Consumer, CallbackLease))
		{
			InGroup.NativeTransaction = Transaction;
			bOutAdvanced = true;
		}
		else RejectNativeAdmission(Admission, Consumer, BeforeAdmission,
			TerrainRenderer->GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked);
		return false;
	}
	const int32 UpdateIndex = InGroup.UpdateIndices[InGroup.NativeSourceCursor];
	const auto& Update = InPublication->Updates[UpdateIndex];
	const ENativeUpdateWork Work = GetNativeUpdateWork(InPublication.Get(), Update);
	if (Work == ENativeUpdateWork::None)
	{
		++InGroup.NativeSourceCursor;
		bOutAdvanced = true;
		return false;
	}
	if (Work == ENativeUpdateWork::Upload && !Update.NativeMesh)
	{
		// 前代回执可能改变显隐基线；重新进入准备，不能在已完成准备的游标上死等。
		InGroup.PreparedUpdates = FMath::Min(InGroup.PreparedUpdates, InGroup.NativeSourceCursor);
		bOutAdvanced = true;
		return false;
	}
	const bool bRemove = Work == ENativeUpdateWork::Remove;
	const auto* Entry = Entries.Find(Update.SourceId);
	if (!Entry)
	{
		DiscardPublication(InPublication, false);
		bOutAdvanced = true;
		return false;
	}
	if (bRemove && (!Entry->bPresented || Entry->bHidden))
	{
		++InGroup.NativeSourceCursor;
		bOutAdvanced = true;
		return false;
	}
	// 移除当前根中的旧几何；本代新源只保留为下一次显示的不可变正文。
	const auto RenderGeometry = bRemove ? Entry->NativeMesh : Update.NativeMesh;
	if (!RenderGeometry || !RenderGeometry->Source)
	{
		DiscardPublication(InPublication, false);
		bOutAdvanced = true;
		return false;
	}
	const auto& RenderSource = RenderGeometry->Source;
	const auto& RenderStamp = RenderGeometry->Stamp;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, InPublication->Serial,
		Update.NativeStamp.GeometryIdentity.Key.Coordinate, Update.NativeStamp.Token);
	FIntVector OwnershipMin;
	FIntVector OwnershipMax;
	const uint64 BeforeRetain = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!RenderStamp.GeometryIdentity.Key.TryBounds(RenderStamp.GeometryIdentity.WorldMinZ,
		RenderStamp.GeometryIdentity.WorldMaxZ, OwnershipMin, OwnershipMax))
	{
		RejectNativeAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, Consumer, BeforeRetain, false);
		return false;
	}
	const FBox OwnershipBounds{FVector(OwnershipMin), FVector(OwnershipMax)};
	const bool bAllowOverlap = TerrainRenderer->HasPresentedCoverage(OwnershipBounds);
	const auto Retain = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (!Lease) return true;
		if (!Lease->IsOwnedBy(Resources.Get()))
		{
			RejectNativeAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, Consumer, BeforeRetain, false);
			return false;
		}
		const auto PreviousPool = Lease->GetPool();
		if (Lease->Transfer(EVoxelTerrainResourcePool::Resident) ||
			(bAllowOverlap && Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap)))
		{
			bOutAdvanced |= Lease->GetPool() != PreviousPool;
			return true;
		}
		const auto Limits = Resources->Snapshot().Limits;
		const bool bFitsPrimary = Lease->Bytes() <= Limits.ResidentBytes - Limits.OverlapBytes;
		const bool bFitsOverlap = bAllowOverlap && Lease->Bytes() <= Limits.OverlapBytes;
		RejectNativeAdmission(bFitsPrimary || bFitsOverlap ? EVoxelTerrainResourceAdmission::CapacityBlocked :
			EVoxelTerrainResourceAdmission::CapacityUnachievable, Consumer, BeforeRetain, false);
		return false;
	};
	if (!InGroup.NativeCoarse)
	{
		// 只对当前实际提交的正文迁池，不代表已经提交或呈现。
		if (!Retain(RenderGeometry->MetadataLease) || !Retain(RenderSource->ResourceLease) ||
			(RenderSource->FrozenVoxelProxy && !Retain(RenderSource->FrozenVoxelProxy->ResourceLease)) ||
			(RenderSource->FrozenSurface && !Retain(RenderSource->FrozenSurface->ResourceLease)) ||
			(RenderSource->FrozenMacro && !Retain(RenderSource->FrozenMacro->ResourceLease)) ||
			(RenderSource->FrozenWater && !Retain(RenderSource->FrozenWater->ResourceLease))) return false;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderCoarse>() +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(RenderSource->Batches.Num());
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredCapacity Required;
		if (!MeasureSources(Required))
		{
			RejectNativeAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable, Consumer, BeforeAdmission, false);
			return false;
		}
		auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Resident, Bytes, Lease, 0,
			Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted && bAllowOverlap)
		{
			const auto OverlapAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::ResidentOverlap, Bytes, Lease, 0,
				Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
			Admission = OverlapAdmission == EVoxelTerrainResourceAdmission::Granted ? OverlapAdmission :
				Admission == EVoxelTerrainResourceAdmission::CapacityBlocked || OverlapAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
				EVoxelTerrainResourceAdmission::CapacityBlocked : OverlapAdmission;
		}
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			RejectNativeAdmission(Admission, Consumer, BeforeAdmission, false);
			return false;
		}
		auto Item = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderCoarse>(Lease);
		Item->MetadataLease = MoveTemp(Lease);
		Item->SourceId = Update.SourceId.Value;
		Item->bBoundaryOnly = bRemove ? Entry->bTransition : Update.bTransition;
		Item->Geometry = RenderGeometry;
		Item->SourceChunks.Reserve(RenderSource->Batches.Num());
		Item->SourceChunks.SetNum(RenderSource->Batches.Num());
		const auto& Input = RenderStamp;
		Item->OwnershipBounds = OwnershipBounds;
		const auto Kind = Input.GeometryIdentity.Key.Representation;
		Item->Evidence.Owner = {Kind == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelFineOwnerKind::Proxy :
			Kind == EVoxelTerrainRepresentation::Surface ? EVoxelFineOwnerKind::Surface : EVoxelFineOwnerKind::Macro,
			Input.OwnerId, Input.OwnerIncarnation};
		Item->Evidence.Content = {WorldEpoch, Input.GeometryIdentity.GeneratorIdentity, Module.GetManifest().RecipeHash,
			Module.GetRegistry()->Hash, Module.GetShapes()->GetContentHash(), Input.Revision, Input.Revision,
			Input.Token, RenderGeometry->GeometryIdentity, Input.GeometryIdentity.SampleStepCells};
		Item->Evidence.bGeometryReady = Item->Evidence.bCanStage = true;
		Item->Evidence.bKnownEmpty = !RenderSource->Batches.ContainsByPredicate([](const auto& Batch) { return !Batch.Mesh.Triangles.IsEmpty(); });
		Item->Evidence.GeometryLease = RenderGeometry;
		InGroup.NativeCoarse = MoveTemp(Item);
		bOutAdvanced = true;
		return false;
	}
	if (!bRemove && Update.Source->Batches.IsValidIndex(InGroup.NativeBatchCursor))
	{
		const int32 Batch = InGroup.NativeBatchCursor;
		const int32 Triangles = Update.Source->Batches[Batch].Mesh.Triangles.Num() / 3;
		if (InGroup.NativeTriangleCursor < Triangles)
		{
			const int32 Count = FMath::Min(4096, Triangles - InGroup.NativeTriangleCursor);
			const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
			EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			auto Chunk = TerrainRenderer->PrepareChunk(Consumer, Update.Source, Batch, Update.Location, Update.Scale,
				InGroup.NativeTriangleCursor, Count, &Admission);
			if (!Chunk)
			{
				RejectNativeAdmission(Admission, Consumer, BeforeAdmission,
					TerrainRenderer->GetNativeStateAdmission() != EVoxelTerrainResourceAdmission::Granted);
				return false;
			}
			TerrainRenderer->AppendSourceChunk(*InGroup.NativeCoarse, Batch, InGroup.NativeLastChunk, MoveTemp(Chunk));
			InGroup.NativeTriangleCursor += Count;
			bOutAdvanced = true;
			return false;
		}
		++InGroup.NativeBatchCursor;
		InGroup.NativeTriangleCursor = 0;
		InGroup.NativeLastChunk.Reset();
		bOutAdvanced = true;
		return false;
	}
	if (!bRemove && !InGroup.NativeCoarse->IsResourceReady()) return false;
	if (!Retain(InGroup.NativeCoarse->MetadataLease)) return false;
	InGroup.NativeChanges.Add({InGroup.NativeCoarse, bRemove});
	InGroup.NativeCoarse.Reset();
	InGroup.NativeLastChunk.Reset();
	InGroup.NativeBatchCursor = InGroup.NativeTriangleCursor = 0;
	++InGroup.NativeSourceCursor;
	bOutAdvanced = true;
	return false;
}
