#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "GameFramework/Actor.h"
#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

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

	struct FNativeMeshWorker
	{
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		FVoxelTerrainCoarseMeshBuilder Builder;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		FVoxelTaskStamp InputStamp;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};

		FNativeMeshWorker(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			const FVoxelGenerationConsumerKey& InConsumer)
			: Resources(InResources), Consumer(InConsumer), Builder(InResources, InConsumer)
		{
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			bWaitingCapacity.Store(false);
			const uint64 BeforeBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			const auto Progress = Builder.Advance(32, InCancel, Result.Error);
			if (Progress == EVoxelTerrainCoarseMeshProgress::Complete)
			{
				Result.CustomPayload = Builder.TakeResult();
				Result.bSuccess = Result.CustomPayload.IsValid();
			}
			else if (Progress == EVoxelTerrainCoarseMeshProgress::Working || Progress == EVoxelTerrainCoarseMeshProgress::CapacityBlocked)
			{
				Result.bSuccess = Result.bContinue = true;
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

bool FVoxelViewPublisher::PrepareUpdate(const FPublicationRef& InPublication, const int32 InIndex,
	const double InDeadline, int32& InOutPreparedComponents, bool& bOutComplete)
{
	(void)InOutPreparedComponents;
	FUpdate& Update = InPublication->Updates[InIndex];
	if (Update.Source && Update.Source->Stamp.bHasGeometryIdentity && Update.Source->Stamp != Update.NativeStamp) return false;
	const bool* Hidden = InPublication->Visibility.Find(Update.Actor);
	const bool bVisibilityChange = Hidden && Update.Actor.IsValid() && Update.Actor->IsHidden() != *Hidden;
	if (!Update.bSourceDirty && !Update.NativeMesh)
		if (const auto* Entry = Entries.Find(Update.Actor); Entry && Entry->NativeMesh &&
			Entry->PresentedSource == Update.Source && Entry->PresentedNativeStamp == Update.NativeStamp &&
			Entry->NativeMesh->WorldCellOrigin == Update.Location / Module.BlockSize() &&
			Entry->NativeMesh->CellScale == Update.Scale / Module.BlockSize()) Update.NativeMesh = Entry->NativeMesh;
	if (Update.NativeMesh && (Update.NativeMesh->Source != Update.Source || Update.NativeMesh->Stamp != Update.NativeStamp ||
		Update.NativeMesh->WorldCellOrigin != Update.Location / Module.BlockSize() ||
		Update.NativeMesh->CellScale != Update.Scale / Module.BlockSize())) Update.NativeMesh.Reset();
	bOutComplete = (Update.bUnchanged && !bVisibilityChange) || Update.NativeMesh.IsValid() || !Update.Actor.IsValid();
	if (bOutComplete || Update.bNativeMeshPending || FPlatformTime::Seconds() >= InDeadline) return true;
	// 保留前代尚未完成的正文，等待该代提交后复用其索引，不能再建同一源。
	if (Update.bRetainSource && !Update.bSourceDirty) return true;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshWorker>(), InputLease);
	if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		const bool bTerminal = InputAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked;
		RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
		return true;
	}
	auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FNativeMeshWorker>(InputLease, Resources, Consumer);
	Worker->InputLease = MoveTemp(InputLease);
	Worker->InputStamp = Update.NativeStamp;
	FString Error;
	if (!Worker->Builder.Initialize(Update.Source, Update.Location / Module.BlockSize(), Update.Scale / Module.BlockSize(), Update.NativeStamp, Error))
	{
		RejectAdmission(true, EVoxelTaskBlockReason::TerminalFailure);
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
	const auto& SourceLease = Update.Source->ResourceLease;
	const bool bOwnSourceLease = SourceLease && SourceLease != Worker->InputLease && SourceLease->IsOwnedBy(Resources.Get());
	const uint64 SourceInputBytes = bOwnSourceLease && SourceLease->GetPool() == EVoxelTerrainResourcePool::Input ? SourceLease->Bytes() : 0;
	if (SourceInputBytes > MAX_uint64 - Worker->InputLease->Bytes())
	{
		RejectAdmission(true, EVoxelTaskBlockReason::Oversized);
		return true;
	}
	Request.SetRequiredBindingInput(Worker->InputLease->Bytes() + SourceInputBytes, 1 + int32(bOwnSourceLease));
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
			[this, InPublication, InIndex, Stamp = Update.NativeStamp](FVoxelTaskResult&& InResult)
	{
		if (InPublication->bCanceled || !InPublication->Updates.IsValidIndex(InIndex)) return;
		auto& Target = InPublication->Updates[InIndex];
		if (Target.NativeStamp != Stamp) return;
		Target.bNativeMeshPending = false;
		if (InResult.bCanceled && InResult.Stamp.WorldEpoch == WorldEpoch && IsNativeGeometryCurrent(Stamp) &&
			!Scheduler.IsTerminal({InResult.Kind, InResult.Stamp}))
		{
			Target.bNativeAdmissionWaiting = false;
			QueueReadyGroup(InPublication, Target.GroupIndex);
			return;
		}
		if (!InResult.bSuccess || InResult.bCanceled || InResult.Stamp.WorldEpoch != WorldEpoch || !IsNativeGeometryCurrent(Stamp))
		{
			const bool bTerminal = Scheduler.IsTerminal({InResult.Kind, InResult.Stamp}) ||
				InResult.Error.StartsWith(TEXT("Oversized")) || InResult.Error.StartsWith(TEXT("CapacityUnachievable"));
			DiscardPublication(InPublication, !bTerminal);
			if (!bTerminal) bRetry = true;
			return;
		}
		Target.NativeMesh = StaticCastSharedPtr<const FVoxelTerrainCoarseMesh>(InResult.CustomPayload);
		if (!Target.NativeMesh)
		{
			DiscardPublication(InPublication);
			bRetry = true;
		}
		else QueueReadyGroup(InPublication, Target.GroupIndex);
	}, CallbackBindingError))
		{
			const bool bTerminal = !CallbackBindingError.StartsWith(TEXT("CapacityBlocked"));
			RejectAdmission(bTerminal, bTerminal ? EVoxelTaskBlockReason::TerminalFailure : EVoxelTaskBlockReason::InputMemory);
			return true;
		}
	}
	Update.bNativeMeshPending = Scheduler.Enqueue(MoveTemp(Request));
	if (!Update.bNativeMeshPending) RejectAdmission(NativePublisherIsTerminalAdmission(Request.Blocked), Request.Blocked);
	return true;
}

bool FVoxelViewPublisher::AdvanceNativeGroup(const FPublicationRef& InPublication, FGroup& InGroup)
{
	if (InGroup.VisibilityActors.IsEmpty() && !InGroup.UpdateIndices.ContainsByPredicate([&InPublication](const int32 Index)
	{
		const auto& Update = InPublication->Updates[Index];
		return Update.Actor.IsValid() && !Update.bUnchanged && Update.bSourceDirty;
	})) return true;
	const auto NativeResources = Scheduler.GetTerrainResources();
	struct FNativeGroupSources
	{
		const FVoxelViewPublisher& Owner;
		const FPublication& Publication;
		const FVoxelTerrainResources& Resources;

		static void Enumerate(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Context = *static_cast<const FNativeGroupSources*>(InContext);
			const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
			{
				if (Lease && Lease->IsOwnedBy(Context.Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
			};
			Context.Owner.TerrainRenderer->VisitNativeControlSources(InVisit);
			for (const auto& Update : Context.Publication.Updates)
			{
				if (Update.Source) FVoxelTerrainRenderer::VisitMeshSources(*Update.Source, Context.Resources, InVisit);
				if (Update.NativeMesh)
				{
					Visit(Update.NativeMesh->MetadataLease);
					if (Update.NativeMesh->Source) FVoxelTerrainRenderer::VisitMeshSources(*Update.NativeMesh->Source, Context.Resources, InVisit);
				}
				if (const auto* Entry = Context.Owner.Entries.Find(Update.Actor); Entry && Entry->NativeMesh)
				{
					Visit(Entry->NativeMesh->MetadataLease);
					if (Entry->NativeMesh->Source) FVoxelTerrainRenderer::VisitMeshSources(*Entry->NativeMesh->Source, Context.Resources, InVisit);
				}
			}
			for (const auto& Group : Context.Publication.Groups)
			{
				Visit(Group.NativeGroupLease);
				if (Group.NativeCoarse) FVoxelTerrainRenderer::VisitCoarseSources(*Group.NativeCoarse, Context.Resources, InVisit);
				for (const auto& Change : Group.NativeChanges)
					if (Change.Item) FVoxelTerrainRenderer::VisitCoarseSources(*Change.Item, Context.Resources, InVisit);
			}
		}
	};
	const FNativeGroupSources SourceContext{*this, InPublication.Get(), NativeResources.Get()};
	const FVoxelGenerationRequiredSourceScope SourceScope(&NativeResources.Get(), &SourceContext, &FNativeGroupSources::Enumerate);
	const auto MeasureSources = [&](FVoxelGenerationRequiredCapacity& OutRequired)
	{
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
	}
	const auto RejectNativeAdmission = [&](const EVoxelTerrainResourceAdmission Admission,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 BeforeAdmission, const bool bBootstrap)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			InGroup.NativeControlConsumer = Consumer;
			InGroup.NativeControlVersion = BeforeAdmission;
			InGroup.NativeFrontendVersion = TerrainRenderer->GetCoarseAdmissionVersion();
			InGroup.bNativeControlWaiting = true;
			InGroup.bNativeBootstrapWaiting = bBootstrap;
		}
		else DiscardPublication(InPublication, false);
	};
	if (InGroup.NativeTransaction.IsValid())
	{
		if (TerrainRenderer->ConsumeTransactionFailure(InGroup.NativeTransaction))
		{
			DiscardPublication(InPublication, false);
			return false;
		}
		if (!InGroup.bNativeSubmitted)
		{
			if (TerrainRenderer->IsTransactionPending(InGroup.NativeTransaction)) return false;
			InGroup.NativeTransaction.Invalidate();
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
			return [this, InPublication, GroupIndex, Lease = MoveTemp(InLease)]
			{
				if (InPublication->bCanceled || !InPublication->Groups.IsValidIndex(GroupIndex)) return false;
				for (const int32 Index : InPublication->Groups[GroupIndex].UpdateIndices)
				{
					if (!InPublication->Updates.IsValidIndex(Index)) return false;
					const auto& Update = InPublication->Updates[Index];
					const bool* Hidden = InPublication->Visibility.Find(Update.Actor);
					if ((!Hidden || !*Hidden) && (!IsNativeGeometryCurrent(Update.NativeStamp) ||
						!Update.Source || (MeshValidator && !MeshValidator(*Update.Source)))) return false;
				}
				return true;
			};
		};
		const auto MakeSubmitted = [&](TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		{
			return [InPublication, GroupIndex, Lease = MoveTemp(InLease)]
			{
				if (InPublication->Groups.IsValidIndex(GroupIndex))
				{
					auto& Group = InPublication->Groups[GroupIndex];
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
			InGroup.NativeTransaction = Transaction;
		else RejectNativeAdmission(Admission, Consumer, BeforeAdmission,
			TerrainRenderer->GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked);
		return false;
	}
	const int32 UpdateIndex = InGroup.UpdateIndices[InGroup.NativeSourceCursor];
	const auto& Update = InPublication->Updates[UpdateIndex];
	if (!Update.Actor.IsValid() || ((!Update.bSourceDirty || Update.bUnchanged) && !InPublication->Visibility.Contains(Update.Actor)))
	{
		++InGroup.NativeSourceCursor;
		return false;
	}
	if (!Update.NativeMesh) return false;
	const bool* Hidden = InPublication->Visibility.Find(Update.Actor);
	const bool bRemove = Hidden && *Hidden;
	const auto* Entry = Entries.Find(Update.Actor);
	if (!Entry) { DiscardPublication(InPublication, false); return false; }
	if (bRemove && (!Entry->bPresented || Update.Actor->IsHidden()))
	{
		++InGroup.NativeSourceCursor;
		return false;
	}
	// 移除当前根中的旧几何；本代新源只保留为下一次显示的不可变正文。
	const auto RenderGeometry = bRemove ? Entry->NativeMesh : Update.NativeMesh;
	if (!RenderGeometry || !RenderGeometry->Source) { DiscardPublication(InPublication, false); return false; }
	const auto& RenderSource = RenderGeometry->Source;
	const auto& RenderStamp = RenderGeometry->Stamp;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, InPublication->Serial,
		Update.NativeStamp.GeometryIdentity.Key.Coordinate, Update.NativeStamp.Token);
	if (!InGroup.NativeCoarse)
	{
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
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease, 0,
			Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			RejectNativeAdmission(Admission, Consumer, BeforeAdmission, false);
			return false;
		}
		auto Item = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderCoarse>(Lease);
		Item->MetadataLease = MoveTemp(Lease);
		Item->SourceId = Entry->NativeSourceId;
		Item->bBoundaryOnly = bRemove ? Entry->bTransition : Update.bTransition;
		Item->Geometry = RenderGeometry;
		Item->SourceChunks.Reserve(RenderSource->Batches.Num());
		Item->SourceChunks.SetNum(RenderSource->Batches.Num());
		const auto& Input = RenderStamp;
		FIntVector Min;
		FIntVector Max;
		if (!Input.GeometryIdentity.Key.TryBounds(Input.GeometryIdentity.WorldMinZ, Input.GeometryIdentity.WorldMaxZ, Min, Max))
		{
			RejectNativeAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, Consumer, BeforeAdmission, false);
			return false;
		}
		Item->OwnershipBounds = FBox(FVector(Min), FVector(Max));
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
			return false;
		}
		++InGroup.NativeBatchCursor;
		InGroup.NativeTriangleCursor = 0;
		InGroup.NativeLastChunk.Reset();
		return false;
	}
	if (!bRemove && !InGroup.NativeCoarse->IsResourceReady()) return false;
	const bool bAllowOverlap = TerrainRenderer->HasPresentedCoverage(InGroup.NativeCoarse->OwnershipBounds);
	const uint64 BeforeRetain = Resources->ReleaseVersionExcludingOwner(Consumer);
	const auto Retain = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (!Lease || Lease->Transfer(EVoxelTerrainResourcePool::Resident) ||
			(bAllowOverlap && Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap))) return true;
		const auto Limits = Resources->Snapshot().Limits;
		const bool bFitsPrimary = Lease->Bytes() <= Limits.ResidentBytes - Limits.OverlapBytes;
		const bool bFitsOverlap = bAllowOverlap && Lease->Bytes() <= Limits.OverlapBytes;
		RejectNativeAdmission(bFitsPrimary || bFitsOverlap ? EVoxelTerrainResourceAdmission::CapacityBlocked :
			EVoxelTerrainResourceAdmission::CapacityUnachievable, Consumer, BeforeRetain, false);
		return false;
	};
	if (!Retain(InGroup.NativeCoarse->MetadataLease) || !Retain(Update.NativeMesh->MetadataLease) ||
		!Retain(Update.Source->ResourceLease)) return false;
	InGroup.NativeChanges.Add({InGroup.NativeCoarse, bRemove});
	InGroup.NativeCoarse.Reset();
	InGroup.NativeLastChunk.Reset();
	InGroup.NativeBatchCursor = InGroup.NativeTriangleCursor = 0;
	++InGroup.NativeSourceCursor;
	return false;
}
