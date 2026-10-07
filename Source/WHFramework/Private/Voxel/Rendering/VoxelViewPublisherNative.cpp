#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "GameFramework/Actor.h"
#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

namespace
{
	struct FNativeMeshWorker
	{
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelTerrainCoarseMeshBuilder Builder;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		FVoxelTaskStamp InputStamp;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};

		FNativeMeshWorker(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			const FVoxelGenerationConsumerKey& InConsumer)
			: Resources(InResources), Builder(InResources, InConsumer)
		{
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			bWaitingCapacity.Store(false);
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
					CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
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
	bOutComplete = Update.NativeMesh.IsValid() || !Update.Actor.IsValid();
	if (bOutComplete || Update.bNativeMeshPending || FPlatformTime::Seconds() >= InDeadline) return true;
	if (!Update.Source || !IsNativeGeometryCurrent(Update.NativeStamp) || (MeshValidator && !MeshValidator(*Update.Source))) return false;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId,
		InPublication->Serial, FIntVector(MIN_int32, 7, InIndex), Update.NativeStamp.Token);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FNativeMeshWorker>(), InputLease)) return true;
	auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FNativeMeshWorker>(InputLease, Resources, Consumer);
	Worker->InputLease = MoveTemp(InputLease);
	Worker->InputStamp = Update.NativeStamp;
	FString Error;
	if (!Worker->Builder.Initialize(Update.Source, Update.Location / Module.BlockSize(), Update.Scale / Module.BlockSize(), Update.NativeStamp, Error)) return true;
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
	SetPublicationPriority(Request, *InPublication, MakeArrayView(&Update.GroupIndex, 1));
	Request.GetBlockReason = [Worker]
	{
		return Worker->bWaitingCapacity.Load() && Worker->Resources->Snapshot().ReleaseVersion <= Worker->CapacityVersion.Load() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Request.Execute = [Worker](const TAtomic<bool>& InCancel) { return Worker->Advance(InCancel); };
	Request.Apply = [this, InPublication, InIndex, Stamp = Update.NativeStamp](FVoxelTaskResult&& InResult)
	{
		if (InPublication->bCanceled || !InPublication->Updates.IsValidIndex(InIndex)) return;
		auto& Target = InPublication->Updates[InIndex];
		if (Target.NativeStamp != Stamp) return;
		Target.bNativeMeshPending = false;
		if (!InResult.bSuccess || InResult.bCanceled || InResult.Stamp.WorldEpoch != WorldEpoch || !IsNativeGeometryCurrent(Stamp))
		{
			DiscardPublication(InPublication);
			bRetry = true;
			return;
		}
		Target.NativeMesh = StaticCastSharedPtr<const FVoxelTerrainCoarseMesh>(InResult.CustomPayload);
		if (!Target.NativeMesh)
		{
			DiscardPublication(InPublication);
			bRetry = true;
		}
		else QueueReadyGroup(InPublication, Target.GroupIndex);
	};
	Update.bNativeMeshPending = Scheduler.Enqueue(MoveTemp(Request));
	return true;
}

bool FVoxelViewPublisher::AdvanceNativeGroup(const FPublicationRef& InPublication, FGroup& InGroup)
{
	if (InGroup.NativeTransaction.IsValid())
	{
		if (!InGroup.bNativeSubmitted)
		{
			if (TerrainRenderer->IsCoarseTransactionPending(InGroup.NativeTransaction)) return false;
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
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainCoarseMutation>(InGroup.UpdateIndices.Num()), InGroup.NativeGroupLease)) return false;
		InGroup.NativeChanges.Reserve(InGroup.UpdateIndices.Num());
	}
	if (!InGroup.UpdateIndices.IsValidIndex(InGroup.NativeSourceCursor))
	{
		if (InGroup.NativeChanges.IsEmpty()) return true;
		const int32 GroupIndex = int32(&InGroup - InPublication->Groups.GetData());
		FGuid Transaction;
		if (TerrainRenderer->PrepareCoarseTransaction(InGroup.NativeChanges,
			[this, InPublication, GroupIndex]
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
			},
			[InPublication, GroupIndex]
			{
				if (InPublication->Groups.IsValidIndex(GroupIndex)) InPublication->Groups[GroupIndex].bNativeSubmitted = true;
			}, Transaction, InGroup.Priority.Service, InGroup.FirstQueuedAt)) InGroup.NativeTransaction = Transaction;
		return false;
	}
	const int32 UpdateIndex = InGroup.UpdateIndices[InGroup.NativeSourceCursor];
	const auto& Update = InPublication->Updates[UpdateIndex];
	if (!Update.NativeMesh) return false;
	const bool* Hidden = InPublication->Visibility.Find(Update.Actor);
	const bool bRemove = Hidden && *Hidden;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, InPublication->Serial,
		Update.NativeStamp.GeometryIdentity.Key.Coordinate, Update.NativeStamp.Token);
	if (!InGroup.NativeCoarse)
	{
		const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelTerrainRenderCoarse>() +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Update.Source->Batches.Num());
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease)) return false;
		auto Item = MakeShared<FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe>();
		Item->MetadataLease = MoveTemp(Lease);
		const auto* Entry = Entries.Find(Update.Actor);
		if (!Entry) return false;
		Item->SourceId = Entry->NativeSourceId;
		Item->bBoundaryOnly = Update.bTransition;
		Item->Geometry = Update.NativeMesh;
		Item->SourceChunks.Reserve(Update.Source->Batches.Num());
		Item->SourceChunks.SetNum(Update.Source->Batches.Num());
		const auto& Input = Update.NativeStamp;
		FIntVector Min;
		FIntVector Max;
		if (!Input.GeometryIdentity.Key.TryBounds(Input.GeometryIdentity.WorldMinZ, Input.GeometryIdentity.WorldMaxZ, Min, Max)) return false;
		Item->OwnershipBounds = FBox(FVector(Min), FVector(Max));
		const auto Kind = Input.GeometryIdentity.Key.Representation;
		Item->Evidence.Owner = {Kind == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelFineOwnerKind::Proxy :
			Kind == EVoxelTerrainRepresentation::Surface ? EVoxelFineOwnerKind::Surface : EVoxelFineOwnerKind::Macro,
			Input.OwnerId, Input.OwnerIncarnation};
		Item->Evidence.Content = {WorldEpoch, Input.GeometryIdentity.GeneratorIdentity, Module.GetManifest().RecipeHash,
			Module.GetRegistry()->Hash, Module.GetShapes()->GetContentHash(), Input.Revision, Input.Revision,
			Input.Token, Update.NativeMesh->GeometryIdentity, Input.GeometryIdentity.SampleStepCells};
		Item->Evidence.bGeometryReady = Item->Evidence.bCanStage = true;
		Item->Evidence.bKnownEmpty = !Update.Source->Batches.ContainsByPredicate([](const auto& Batch) { return !Batch.Mesh.Triangles.IsEmpty(); });
		Item->Evidence.GeometryLease = Update.NativeMesh;
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
			auto Chunk = TerrainRenderer->PrepareChunk(Consumer, Update.Source, Batch, Update.Location, Update.Scale,
				InGroup.NativeTriangleCursor, Count);
			if (!Chunk) return false;
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
	const auto Retain = [bAllowOverlap](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		return !Lease || Lease->Transfer(EVoxelTerrainResourcePool::Resident) ||
			(bAllowOverlap && Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap));
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
