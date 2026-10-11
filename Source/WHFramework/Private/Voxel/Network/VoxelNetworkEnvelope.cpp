#include "Voxel/Network/VoxelNetworkEnvelope.h"

#include "Misc/Crc.h"
#include "Async/Async.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> FVoxelNetworkEnvelopeDecode::Create(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
	TArray<uint8>&& InBytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>&& InInputLease,
	const uint32 InCrc, const bool bInServer, FString& OutError,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>&& InMetadataLease)
{
	if (!InConsumer.IsValid() || InBytes.IsEmpty() || InBytes.Num() > FVoxelNetworkCodec::MaxWireBytes ||
		!InInputLease || !InInputLease->IsOwnedBy(InResources.Get()) || InInputLease->GetPool() != EVoxelTerrainResourcePool::Input ||
		VoxelGenerationAllocation::AllocatedBytes(InBytes) > InInputLease->Bytes() ||
		(InResources->GetControlConsumer().IsValid() && InConsumer.WorldEpoch != InResources->GetControlConsumer().WorldEpoch))
	{
		OutError = TEXT("完整 Envelope 缺少有界输入租约");
		return nullptr;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata = InMetadataLease;
	if (Metadata && (!Metadata->IsOwnedBy(InResources.Get()) || Metadata->GetPool() != EVoxelTerrainResourcePool::Input ||
		Metadata->Bytes() != VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>()))
	{
		OutError = TEXT("InvalidResource: Envelope original metadata reservation");
		return nullptr;
	}
	if (!Metadata)
	{
		FVoxelGenerationRequiredSourceScope InputSource(&InResources.Get(), InInputLease.Get());
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Required, OutError)) return nullptr;
		const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>(), Metadata, 0,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Envelope 输入元数据") :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("Oversized: Envelope 输入元数据") :
				TEXT("InvalidResource: Envelope consumer");
			return nullptr;
		}
	}
	auto Work = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNetworkEnvelopeDecode>(Metadata);
	Work->Resources = InResources;
	Work->Consumer = InConsumer;
	Work->Bytes = MoveTemp(InBytes);
	Work->InputLease = MoveTemp(InInputLease);
	Work->MetadataLease = MoveTemp(Metadata);
	InMetadataLease.Reset();
	Work->ExpectedCrc = InCrc;
	Work->bServer = bInServer;
	// 连接队列的保守上界不作为资源授予；Worker 验证原 wire 后按实际布局计数。
	Work->IngressQuotaBytes = Work->InputLease->Bytes() + Work->MetadataLease->Bytes() +
		2 * VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Work->Bytes.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(FVoxelOverlayRegionCodec::MaximumRegions) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayDirectoryEntry, FDefaultAllocator>(FVoxelOverlayRegionCodec::MaximumPageEntries) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(FVoxelOverlayRegionCodec::MaximumPartBytes) +
		sizeof(FVoxelOverlayRegionMessage) + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>();
	OutError.Reset();
	return Work;
}

FVoxelTaskResult FVoxelNetworkEnvelopeDecode::Advance(const TAtomic<bool>& InCancel)
{
	FVoxelTaskResult Result;
	if (InCancel.Load()) { Result.bCanceled = true; return Result; }
	if (bComplete) { Result.bSuccess = true; Result.CustomPayload = AsShared(); return Result; }
	bWaitingCapacity = false;
	for (int32 Step = 0; Step < 16 && VerifiedBytes < Bytes.Num(); ++Step)
	{
		if (InCancel.Load()) { Result.bCanceled = true; return Result; }
		const int32 Count = FMath::Min(4096, Bytes.Num() - VerifiedBytes);
		RunningCrc = FCrc::MemCrc32(Bytes.GetData() + VerifiedBytes, Count, RunningCrc);
		VerifiedBytes += Count;
	}
	if (VerifiedBytes < Bytes.Num()) { Result.bSuccess = Result.bContinue = true; return Result; }
	if (RunningCrc != ExpectedCrc) { Result.Error = TEXT("完整传输正文 CRC 不匹配"); return Result; }
	if (!bMeasured)
	{
		EVoxelMessage Kind;
		FGuid Session;
		TConstArrayView<uint8> Payload;
		if (!FVoxelNetworkCodec::DecodeView(Bytes, Kind, Session, Payload))
		{
			Result.Error = TEXT("Voxel V4 Envelope 格式不合法");
			return Result;
		}
		bOverlay = Kind >= EVoxelMessage::SubscribeRegion && Kind <= EVoxelMessage::ResyncRequest;
		bRepresentationReply = !bServer && Kind == EVoxelMessage::RepresentationReply;
		DecodeBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Payload.Num());
		if (bOverlay)
		{
			uint64 OverlayBytes = 0;
			if (!FVoxelOverlayRegionCodec::MeasureDecode(Kind, Payload, OverlayBytes, Result.Error)) return Result;
			DecodeBytes += OverlayBytes;
		}
		if (bRepresentationReply)
		{
			FVoxelRepresentationReply Scalars;
			TConstArrayView<uint8> Data;
			if (!FVoxelNetworkCodec::DecodeRepresentationReplyView(Payload, Scalars, Data, Result.Error)) return Result;
			DecodeBytes += VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Data.Num()) +
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>();
		}
		bMeasured = true;
	}
	if (!DecodeLease && DecodeBytes)
	{
		CapacityVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
		const FVoxelGenerationRequiredSourceScope InputSource(Resources.Get(), InputLease.Get());
		const FVoxelGenerationRequiredSourceScope MetadataSource(Resources.Get(), MetadataLease.Get());
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Result.Error)) return Result;
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, DecodeBytes, DecodeLease,
			int32(bOverlay) + int32(bRepresentationReply), Required.Bytes[0], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (Admission != EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				Result.Error = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: Envelope 解码原生布局不可接纳") : TEXT("InvalidResource: Envelope 解码授予");
				return Result;
			}
			bWaitingCapacity = true;
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}
	}
	if (!bDecoded)
	{
		if (!FVoxelNetworkCodec::Decode(Bytes, Message)) { Result.Error = TEXT("Voxel V4 Envelope 格式不合法"); return Result; }
		bOverlay = Message.Kind >= EVoxelMessage::SubscribeRegion && Message.Kind <= EVoxelMessage::ResyncRequest;
		if (bOverlay && !FVoxelOverlayRegionCodec::Decode(Message.Kind, Message.Payload, Overlay, Result.Error)) return Result;
		bRepresentationReply = !bServer && Message.Kind == EVoxelMessage::RepresentationReply;
		if (bRepresentationReply && !FVoxelNetworkCodec::DecodeRepresentationReply(Message.Payload, RepresentationReply, Result.Error)) return Result;
		const uint64 OverlayBytes = bOverlay ? sizeof(FVoxelOverlayRegionMessage) +
			VoxelGenerationAllocation::AllocatedBytes(Overlay.Heads) + VoxelGenerationAllocation::AllocatedBytes(Overlay.Entries) +
			VoxelGenerationAllocation::AllocatedBytes(Overlay.Payload) : 0;
		const uint64 RepresentationBytes = bRepresentationReply ? VoxelGenerationAllocation::AllocatedBytes(RepresentationReply.Data) +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() : 0;
		const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Message.Payload) + OverlayBytes + RepresentationBytes;
		if (ActualBytes > DecodeBytes || (DecodeLease && !DecodeLease->Resize(ActualBytes)))
		{
			Result.Error = TEXT("Envelope 解码输出超过分配前授予");
			return Result;
		}
		if (bOverlay)
		{
			OverlayLease = DecodeLease->Split(OverlayBytes);
			check(OverlayLease);
			Overlay.ResourceLease = OverlayLease;
		}
		if (bRepresentationReply)
		{
			RepresentationLease = DecodeLease->Split(RepresentationBytes);
			check(RepresentationLease);
			auto Body = VoxelBoundedAllocation::MakeWorkerShared<FVoxelRepresentationReplyData>(RepresentationLease);
			Body->Bytes = MoveTemp(RepresentationReply.Data);
			Body->Lease = RepresentationLease;
			RepresentationReply.SharedData = MoveTemp(Body);
		}
		bDecoded = true;
	}
	if (DecodeLease) DecodeLease->MarkPublished();
	if (OverlayLease) OverlayLease->MarkPublished();
	if (RepresentationLease) RepresentationLease->MarkPublished();
	bComplete = Result.bSuccess = true;
	Result.CustomPayload = AsShared();
	return Result;
}

EVoxelTaskBlockReason FVoxelNetworkEnvelopeDecode::GetBlockReason() const
{
	uint64 Released = 0;
	return bWaitingCapacity && (!Resources->TryGetReleaseVersionExcludingOwner(Consumer, Released) || Released <= CapacityVersion) ?
		EVoxelTaskBlockReason::InputMemory : EVoxelTaskBlockReason::None;
}

uint64 FVoxelNetworkEnvelopeDecode::GetIngressQuotaBytes() const
{
	return IngressQuotaBytes;
}

uint64 FVoxelNetworkEnvelopeDecode::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>() +
		Bytes.GetAllocatedSize() +
		Message.Payload.GetAllocatedSize() + Overlay.Heads.GetAllocatedSize() +
		Overlay.Entries.GetAllocatedSize() + Overlay.Payload.GetAllocatedSize() +
		(RepresentationReply.SharedData ? RepresentationReply.SharedData->Bytes.GetAllocatedSize() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() : 0);
}

uint64 FVoxelNetworkEnvelopeDecode::GetResourceAccountedBytes() const
{
	return InputLease->Bytes() + MetadataLease->Bytes() + (DecodeLease ? DecodeLease->Bytes() : 0) +
		(OverlayLease ? OverlayLease->Bytes() : 0) + (RepresentationLease ? RepresentationLease->Bytes() : 0);
}
