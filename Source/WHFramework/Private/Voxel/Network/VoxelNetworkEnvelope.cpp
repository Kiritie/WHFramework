#include "Voxel/Network/VoxelNetworkEnvelope.h"

#include "Misc/Crc.h"
#include "Async/Async.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> FVoxelNetworkEnvelopeDecode::Create(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer,
	TArray<uint8>&& InBytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InInputLease,
	const uint32 InCrc, const bool bInServer, FString& OutError)
{
	if (!InConsumer.IsValid() || InBytes.IsEmpty() || InBytes.Num() > FVoxelNetworkCodec::MaxWireBytes ||
		!InInputLease || uint64(InBytes.GetAllocatedSize()) > InInputLease->Bytes())
	{
		OutError = TEXT("完整 Envelope 缺少有界输入租约");
		return nullptr;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>(), Metadata))
	{
		OutError = TEXT("CapacityBlocked: Envelope 输入元数据");
		return nullptr;
	}
	auto Work = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNetworkEnvelopeDecode>(Metadata);
	Work->Resources = InResources;
	Work->Consumer = InConsumer;
	Work->Bytes = MoveTemp(InBytes);
	Work->InputLease = MoveTemp(InInputLease);
	Work->MetadataLease = MoveTemp(Metadata);
	Work->ExpectedCrc = InCrc;
	Work->bServer = bInServer;
	// Decode 的临时正文与结果同时存在；区域数组及 Part 正文也在授予之后分配。
	Work->DecodeBytes = VoxelBoundedAllocation::InitialGrowBytes<uint8>(Work->Bytes.Num()) * 2 +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(FVoxelOverlayRegionCodec::MaximumRegions) * 2 +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(FVoxelOverlayRegionCodec::MaximumPageEntries) * 2 +
		VoxelBoundedAllocation::InitialGrowBytes<uint8>(FVoxelOverlayRegionCodec::MaximumPartBytes) * 2 +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() + 1024;
	Work->IngressQuotaBytes = Work->InputLease->Bytes() + Work->MetadataLease->Bytes() + Work->DecodeBytes;
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
	if (!DecodeLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, DecodeBytes, DecodeLease, 1))
	{
		CapacityVersion = Resources->Snapshot().ReleaseVersion;
		bWaitingCapacity = true;
		Result.bSuccess = Result.bContinue = true;
		return Result;
	}
	if (!bDecoded)
	{
		if (!FVoxelNetworkCodec::Decode(Bytes, Message)) { Result.Error = TEXT("Voxel V4 Envelope 格式不合法"); return Result; }
		bOverlay = Message.Kind >= EVoxelMessage::SubscribeRegion && Message.Kind <= EVoxelMessage::ResyncRequest;
		if (bOverlay && !FVoxelOverlayRegionCodec::Decode(Message.Kind, Message.Payload, Overlay, Result.Error)) return Result;
		bRepresentationReply = !bServer && Message.Kind == EVoxelMessage::RepresentationReply;
		if (bRepresentationReply && !FVoxelNetworkCodec::DecodeRepresentationReply(Message.Payload, RepresentationReply, Result.Error)) return Result;
		const uint64 OverlayBytes = bOverlay ? Overlay.AllocatedBytes() : 0;
		const uint64 RepresentationBytes = bRepresentationReply ? RepresentationReply.Data.GetAllocatedSize() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() : 0;
		const uint64 ActualBytes = Message.Payload.GetAllocatedSize() + OverlayBytes + RepresentationBytes;
		if (ActualBytes > DecodeLease->Bytes() || !DecodeLease->Resize(ActualBytes))
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
	if ((OverlayLease && !OverlayLease->Transfer(EVoxelTerrainResourcePool::Input)) ||
		(RepresentationLease && !RepresentationLease->Transfer(EVoxelTerrainResourcePool::Input)))
	{
		CapacityVersion = Resources->Snapshot().ReleaseVersion;
		bWaitingCapacity = true;
		Result.bSuccess = Result.bContinue = true;
		return Result;
	}
	bComplete = Result.bSuccess = true;
	Result.CustomPayload = AsShared();
	return Result;
}

EVoxelTaskBlockReason FVoxelNetworkEnvelopeDecode::GetBlockReason() const
{
	return bWaitingCapacity && Resources->Snapshot().ReleaseVersion <= CapacityVersion ?
		EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
}

uint64 FVoxelNetworkEnvelopeDecode::GetIngressQuotaBytes() const
{
	return IngressQuotaBytes;
}

uint64 FVoxelNetworkEnvelopeDecode::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>() +
		Bytes.GetAllocatedSize() +
		Message.Payload.GetAllocatedSize() + Overlay.AllocatedBytes() +
		(RepresentationReply.SharedData ? RepresentationReply.SharedData->Bytes.GetAllocatedSize() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() : 0);
}

uint64 FVoxelNetworkEnvelopeDecode::GetResourceAccountedBytes() const
{
	return InputLease->Bytes() + MetadataLease->Bytes() + (DecodeLease ? DecodeLease->Bytes() : 0) +
		(OverlayLease ? OverlayLease->Bytes() : 0) + (RepresentationLease ? RepresentationLease->Bytes() : 0);
}
