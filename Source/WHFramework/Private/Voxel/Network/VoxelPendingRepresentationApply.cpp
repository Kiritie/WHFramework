#include "Voxel/Network/VoxelModuleNetworkComponent.h"

#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "VoxelModuleNetworkOverlayState.h"
#include "VoxelRepresentationSubscriptionState.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/VoxelModule.h"

namespace
{
uint64 RetainedRepresentationBytes(const FVoxelRepresentationReply& Reply)
	{
		return Reply.SharedData && Reply.SharedData->Lease ? Reply.SharedData->Lease->Bytes() : Reply.Data.GetAllocatedSize();
}
}

void UVoxelModuleNetworkComponent::AdvanceRepresentationSubscriptions(const double InNow, const double InDeadline)
{
	(void)InNow;
	auto* WorldModule = Module.Get();
	auto* Observer = GetAuthorizedObserver();
	if (!WorldModule || !Observer || !RepresentationSubscriptions || !OverlayNetwork) return;
	auto& State = *RepresentationSubscriptions;
	const FVector ObserverCell = Observer->GetActorLocation() / WorldModule->BlockSize();
	const FIntVector Position(FMath::FloorToInt(ObserverCell.X / 64.0), FMath::FloorToInt(ObserverCell.Y / 64.0),
		FMath::FloorToInt(ObserverCell.Z / 64.0));
	if (!State.bRetiring && State.RetirementPosition != Position)
	{
		State.RetirementPosition = Position;
		State.RetirementCursor = 0;
		State.bRetiring = true;
		State.RetirementContinuation = State.Index.CaptureAllSteps(State.Retiring, false);
	}
	for (int32 Step = 0; Step < 32 && State.bRetiring && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		if (!State.RetirementContinuation.IsComplete())
		{
			State.RetirementContinuation.Resume();
			continue;
		}
		State.RetirementContinuation.Reset();
		if (State.RetirementCursor >= State.Retiring.Records.Num())
		{
			State.Retiring = {};
			State.bRetiring = false;
			break;
		}
		const auto Record = State.Retiring.Records[State.RetirementCursor];
		const auto Type = static_cast<EVoxelRepresentationWireType>(uint8(Record->Geometry.Key.Representation) - 1);
		if (!IsRepresentationInRange(ObserverCell, Type, {Record->Min, Record->Max}, WorldModule->GetViewSettings()))
		{
			const auto Current = State.Index.Find(Record->Geometry.Key);
			FString Error;
			if (Current && Current->Geometry == Record->Geometry && !State.Index.Remove(Record->Geometry.Key, Error)) break;
		}
		++State.RetirementCursor;
	}
	for (int32 Step = 0; Step < 64 && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		if (!State.PendingWire.IsEmpty())
		{
			if (!Send(EVoxelMessage::RepresentationInvalidate, State.PendingWire, EVoxelTransferPriority::Normal)) return;
			State.PendingWire.Empty();
			State.WireLease.Reset();
			continue;
		}
		if (!State.bActiveCut)
		{
			if (State.AcceptedCut.ContentRevision == State.LatestCut.ContentRevision) return;
			State.ActiveCut = State.LatestCut;
			State.ActiveCut.SectionChanges.BindResources(OverlayNetwork->Resources.ToSharedRef(),
				FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId, TaskOwnerIncarnation, {}, ++NextTaskSerial));
			State.ChangesContinuation = State.ActiveCut.SectionChanges.CaptureChangedSteps(State.AcceptedCut.SectionChanges, State.Changes);
			State.ChangeCursor = State.LocalCursor = 0;
			State.bActiveCut = true;
		}
		if (!State.ChangesContinuation.IsComplete())
		{
			State.ChangesContinuation.Resume();
			continue;
		}
		State.ChangesContinuation.Reset();
		if (!State.LocalContinuation.IsComplete())
		{
			State.LocalContinuation.Resume();
			continue;
		}
		if (State.LocalCursor < State.Local.Records.Num())
		{
			const auto Record = State.Local.Records[State.LocalCursor];
			const auto Current = State.Index.Find(Record->Geometry.Key);
			const auto& Change = State.Changes.Records[State.ChangeCursor];
			const int64 Halo = Record->Geometry.SampleStepCells;
			const int64 Horizontal = Halo + State.ActiveCut.NaturalHorizontalInfluence;
			const bool bAffected = int64(Record->Min.X) - Horizontal < Change->Max.X && int64(Record->Max.X) + Horizontal > Change->Min.X &&
				int64(Record->Min.Y) - Horizontal < Change->Max.Y && int64(Record->Max.Y) + Horizontal > Change->Min.Y &&
				int64(Record->Min.Z) - Halo - State.ActiveCut.NaturalUpwardInfluence < Change->Max.Z && int64(Record->Max.Z) + Halo > Change->Min.Z;
			if (!Current || Current->Geometry != Record->Geometry || !bAffected)
			{
				++State.LocalCursor;
				continue;
			}
			const uint64 Revision = State.ActiveCut.GetGeometryRevision(Record->Geometry);
			if (Revision == 0) { ++State.LocalCursor; continue; }
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
			const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
				TaskOwnerIncarnation, {}, ++NextTaskSerial);
			const uint64 WireBytes = VoxelBoundedAllocation::ArrayBytes<uint8>(2 + VoxelGeometryWire::HeaderBytes);
			if (!State.WireLease && !OverlayNetwork->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, WireBytes, State.WireLease)) return;
			if (!OverlayNetwork->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
				WireBytes + VoxelBoundedAllocation::ArrayBytes<uint8>(VoxelGeometryWire::HeaderBytes) +
				VoxelBoundedAllocation::ArrayBytes<FVoxelRepresentationInvalidation>(1), Scratch)) return;
			FVoxelRepresentationInvalidate Message;
			Message.Entries.Reserve(1);
			Message.Entries.Add({Record->Geometry, Revision});
			FString Error;
			if (!FVoxelNetworkCodec::EncodeRepresentationInvalidate(Message, State.PendingWire, Error)) { Fail(Error); return; }
			++State.LocalCursor;
			continue;
		}
		if (State.LocalContinuation.IsValid())
		{
			State.LocalContinuation.Reset();
			State.Local = {};
			++State.ChangeCursor;
		}
		if (State.ChangeCursor >= State.Changes.Records.Num())
		{
			State.AcceptedCut = State.ActiveCut;
			State.ActiveCut = {};
			State.Changes = {};
			State.bActiveCut = false;
			continue;
		}
		const auto& Change = State.Changes.Records[State.ChangeCursor];
		const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
		const int64 Horizontal = int64(State.MaximumHaloStep) + State.ActiveCut.NaturalHorizontalInfluence;
		State.LocalCursor = 0;
		State.LocalContinuation = State.Index.CaptureSteps(
			FIntVector(Cell(int64(Change->Min.X) - Horizontal), Cell(int64(Change->Min.Y) - Horizontal), Cell(int64(Change->Min.Z) - State.MaximumHaloStep)),
			FIntVector(Cell(int64(Change->Max.X) + Horizontal), Cell(int64(Change->Max.Y) + Horizontal),
				Cell(int64(Change->Max.Z) + State.MaximumHaloStep + State.ActiveCut.NaturalUpwardInfluence)), State.Local, false);
	}
}

bool UVoxelModuleNetworkComponent::ReserveRepresentationTables()
{
	if (!OverlayNetwork || NetworkSettings.MaxRepresentationRequests < 1) return false;
	auto* WorldModule = Module.Get();
	if (!WorldModule || !WorldModule->GetRuntime()) return false;
	if (GetOwner() && GetOwner()->HasAuthority() && !RepresentationSubscriptions)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		if (!OverlayNetwork->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
			FMemory::QuantizeSize(sizeof(FVoxelRepresentationSubscriptionState), alignof(FVoxelRepresentationSubscriptionState)), Lease)) return false;
		RepresentationSubscriptions = MakeUnique<FVoxelRepresentationSubscriptionState>();
		RepresentationSubscriptions->Lease = MoveTemp(Lease);
		RepresentationSubscriptions->Index.BindResources(OverlayNetwork->Resources.ToSharedRef(), Consumer);
		RepresentationSubscriptions->AcceptedCut = WorldModule->GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot();
		RepresentationSubscriptions->LatestCut = RepresentationSubscriptions->AcceptedCut;
	}
	if (NetworkSettings.MaxRepresentationRequests <= RepresentationTableCapacity) return true;
	const int64 Proposed = FMath::Max<int64>(NetworkSettings.MaxRepresentationRequests,
		FMath::Max<int64>(8, int64(RepresentationTableCapacity) * 2));
	if (Proposed > MAX_int32) return false;
	const int32 Capacity = int32(Proposed);
	const uint64 Bytes = VoxelBoundedAllocation::MapBytes<uint64, FPendingRepresentation>(Capacity);
	const uint64 Previous = PendingRepresentationRequests.GetAllocatedSize();
	if (RepresentationTableLease)
	{
		if (!RepresentationTableLease->Resize(Previous + Bytes)) return false;
	}
	else
	{
		if (!WorldModule || !OverlayNetwork->Resources->Acquire(
			FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId, TaskOwnerIncarnation, {}, ++NextTaskSerial),
			EVoxelTerrainResourcePool::Resident, Previous + Bytes, RepresentationTableLease)) return false;
	}
	PendingRepresentationRequests.Reserve(Capacity);
	check(PendingRepresentationRequests.GetAllocatedSize() <= Bytes);
	verify(RepresentationTableLease->Resize(Bytes));
	RepresentationTableCapacity = Capacity;
	return true;
}

void UVoxelModuleNetworkComponent::ResetRepresentationRequests()
{
	// 数量由每连接配置约束；大正文最后一次释放由 Worker-owned backing 接管。
	PendingRepresentationRequests.Empty();
	RepresentationSubscriptions.Reset();
	PendingRepresentationBytes = 0;
	PendingRepresentationCursor = 0;
	RepresentationTableCapacity = 0;
	RepresentationTableLease.Reset();
}

bool UVoxelModuleNetworkComponent::TryApplyRepresentationReply(const uint64 InSerial)
{
	auto* Pending = PendingRepresentationRequests.Find(InSerial);
	auto* WorldModule = Module.Get();
	if (!Pending || !Pending->bHasReply || !WorldModule) return false;
	const auto Remove = [this, InSerial, Pending]
	{
		const uint64 Bytes = RetainedRepresentationBytes(Pending->Reply);
		check(PendingRepresentationBytes >= Bytes);
		PendingRepresentationBytes -= Bytes;
		PendingRepresentationRequests.Remove(InSerial);
	};
	const auto& Request = Pending->Request;
	const FVoxelTerrainSpatialKey Key{static_cast<EVoxelTerrainRepresentation>(uint8(Request.Type) + 1), Request.Key.Coordinate, Request.Key.Level};
	const auto* Geometry = WorldModule->GetCurrentInterest().Geometry.Find(Key);
	if (Pending->RequestSession != Session || Pending->WorldEpoch != WorldModule->GetWorldEpoch() ||
		Pending->OwnerIncarnation != TaskOwnerIncarnation || !Geometry || *Geometry != Request.Geometry)
	{
		Remove();
		return true;
	}
	const auto& Hierarchy = WorldModule->GetRuntime()->GetChangeHierarchy();
	const uint64 RequiredRevision = Request.Type == EVoxelRepresentationWireType::VoxelProxy ?
		Hierarchy.GetVoxelProxyRevision({Request.Key.Coordinate, Request.Key.Level}) :
		Request.Type == EVoxelRepresentationWireType::SurfaceProxy ?
		Hierarchy.GetSurfaceRevision({FIntPoint(Request.Key.Coordinate.X, Request.Key.Coordinate.Y), Request.Key.Level}) :
		Hierarchy.GetMacroRevision({FIntPoint(Request.Key.Coordinate.X, Request.Key.Coordinate.Y), Request.Key.Level});
	if ((Pending->Reply.Kind == EVoxelRepresentationReplyKind::Data || Pending->Reply.Kind == EVoxelRepresentationReplyKind::Unchanged) &&
		Pending->Reply.Revision < FMath::Max(Request.KnownRevision, RequiredRevision))
	{
		Remove();
		return true;
	}
	FString Error;
	if (!WorldModule->ApplyRemoteRepresentation(Pending->Reply, Error))
	{
		if (Error == TEXT("Remote terrain input capacity deferred") || Error == TEXT("Remote terrain admission deferred"))
		{
			// 保留原请求和已验证正文；Overlay 的连续目录/增量控制流仍可独立推进。
			Pending->CapacityVersion = WorldModule->GetTerrainResources()->Snapshot().ReleaseVersion;
			return false;
		}
		Fail(Error.IsEmpty() ? TEXT("Voxel representation reply could not be applied") : Error);
		return false;
	}
	// 生产解码已使用共享正文；回调的临时 pin 只复制元数据和共享引用。
	const FVoxelRepresentationReply Reply = Pending->Reply;
	Remove();
	OnRepresentationReply.Broadcast(Reply);
	return true;
}

void UVoxelModuleNetworkComponent::AdvancePendingRepresentations(const double InDeadline)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || bRejected || !bReady) return;
	const uint64 ReleaseVersion = WorldModule->GetTerrainResources()->Snapshot().ReleaseVersion;
	const double Now = FPlatformTime::Seconds();
	const int32 Maximum = PendingRepresentationRequests.GetMaxIndex();
	for (int32 Step = 0; Step < FMath::Min(32, Maximum) && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		if (PendingRepresentationCursor >= Maximum) PendingRepresentationCursor = 0;
		const auto Id = FSetElementId::FromInteger(PendingRepresentationCursor++);
		if (!PendingRepresentationRequests.IsValidId(Id)) continue;
		auto& Entry = PendingRepresentationRequests.Get(Id);
		const uint64 Serial = Entry.Key;
		const auto& Request = Entry.Value.Request;
		const FVoxelTerrainSpatialKey Key{static_cast<EVoxelTerrainRepresentation>(uint8(Request.Type) + 1), Request.Key.Coordinate, Request.Key.Level};
		const auto* Geometry = WorldModule->GetCurrentInterest().Geometry.Find(Key);
		if (Entry.Value.RequestSession != Session || Entry.Value.WorldEpoch != WorldModule->GetWorldEpoch() ||
			Entry.Value.OwnerIncarnation != TaskOwnerIncarnation || !Geometry || *Geometry != Request.Geometry)
		{
			const uint64 Bytes = Entry.Value.bHasReply ? RetainedRepresentationBytes(Entry.Value.Reply) : 0;
			check(PendingRepresentationBytes >= Bytes);
			PendingRepresentationBytes -= Bytes;
			PendingRepresentationRequests.Remove(Serial);
			continue;
		}
		if (!Entry.Value.bHasReply)
		{
			// 保留同一 Serial/Geometry 重传；迟到的重复回复只能满足原 Attempt。
			if (Now - Entry.Value.LastSentSeconds >= 15.0)
			{
				TArray<uint8> Payload;
				if (FVoxelNetworkCodec::EncodeRepresentationRequest(Request, Payload) &&
					Send(EVoxelMessage::RepresentationRequest, Payload, EVoxelTransferPriority::Normal)) Entry.Value.LastSentSeconds = Now;
			}
			continue;
		}
		if (Entry.Value.CapacityVersion && ReleaseVersion <= Entry.Value.CapacityVersion) continue;
		TryApplyRepresentationReply(Serial);
		if (bRejected) return;
	}
}
