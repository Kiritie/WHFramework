#pragma once

#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Misc/Crc.h"

struct FVoxelServerRepresentationResult final : FVoxelTaskCustomPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<uint8> Envelope;
	uint32 Crc = 0;
	virtual uint64 GetAllocatedBytes() const override
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelServerRepresentationResult>() + Envelope.GetAllocatedSize();
	}
	virtual uint64 GetResourceAccountedBytes() const override { return Lease ? Lease->Bytes() : 0; }
};

// 生成、Reply、Envelope 和 CRC 在 Worker 完成；GT 只接纳已编码正文。
struct FVoxelServerRepresentationBuild
{
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease, CompletionLease;
	FVoxelBinaryBuffer EncodedData;
	FVoxelRepresentationPageBuild Page;
	FVoxelRepresentationReply Reply;
	FGuid Session;
	TAtomic<bool> bWaitingCapacity{false};
	TAtomic<uint64> CapacityVersion{0};
	uint64 CompletionBytes = 0;

	FVoxelServerRepresentationBuild(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
		: Resources(MoveTemp(InResources)), Consumer(InConsumer), Page(Resources, Consumer) {}

	bool Initialize(FVoxelRepresentationBuildInput&& Input, const FVoxelRepresentationReply& InReply,
		const FGuid& InSession, FString& Error)
	{
		if (!InReply.DataView().IsEmpty() || !InSession.IsValid() || InReply.Type != Input.Request.Type ||
			InReply.Key != Input.Request.Key || InReply.Serial != Input.Request.Serial)
		{ Error = TEXT("Invalid original server representation reply identity"); return false; }
		Reply = InReply; Session = InSession;
		if (!Page.Initialize(MoveTemp(Input), Error)) return false;
		const uint64 MaximumWireBytes = Page.MaximumWireBytes();
		if (MaximumWireBytes == 0)
		{ Error = TEXT("Oversized: server representation has no valid wire bound"); return false; }
		// 在生成前接纳实际空间/采样推导的线上上界；完成不会被其它输入挤出 Staged。
		CompletionBytes = Page.RawCapacityBytes() +
			VoxelBoundedAllocation::ArrayBytes<uint8>(int32(MaximumWireBytes) + 35) +
			VoxelBoundedAllocation::ArrayBytes<uint8>(int32(MaximumWireBytes) + 64) +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelServerRepresentationResult>();
		if (CompletionBytes > Resources->Snapshot().Limits.StagedBytes)
		{ Error = TEXT("Oversized: server representation completion"); return false; }
		return true;
	}

	EVoxelTaskBlockReason BlockReason() const
	{
		if (!bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
		uint64 Version = 0;
		return !Resources->TryGetReleaseVersionExcludingOwner(Consumer, Version) || Version <= CapacityVersion.Load() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load()) { Result.bCanceled = true; return Result; }
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
		const auto Block = [&]
		{
			Working.Reset();
			CapacityVersion.Store(Resources->ReleaseVersionExcludingOwner(Consumer)); bWaitingCapacity.Store(true);
			Result.Error.Reset(); Result.bSuccess = Result.bContinue = true;
		};
		if (!CompletionLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, CompletionBytes, CompletionLease))
		{ Block(); return Result; }
		bWaitingCapacity.Store(false);
		if (EncodedData.Bytes.IsEmpty())
		{
			// 原生成窗口的保留模型；网络 codec 的正文和 native zlib 均单独按真实大小授予。
			if (Page.NeedsGeneration() &&
				!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, 24ull * 1024 * 1024, Working))
			{ Block(); return Result; }
			bool Complete = false;
			if (!Page.Advance(EncodedData, Complete, Result.Error, &Cancel))
			{
				if (Result.Error.StartsWith(TEXT("CapacityBlocked"))) Block();
				else Result.bCanceled = Cancel.Load();
				return Result;
			}
			if (!Complete) { Result.bSuccess = Result.bContinue = true; return Result; }
		}
		Working.Reset();
		const uint64 EncodingBytes = VoxelBoundedAllocation::ArrayBytes<uint8>(EncodedData.Bytes.Num() + 35) +
			VoxelBoundedAllocation::ArrayBytes<uint8>(EncodedData.Bytes.Num() + 64) +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelServerRepresentationResult>();
		check(EncodingBytes <= CompletionLease->Bytes());
		Reply.Kind = EVoxelRepresentationReplyKind::Data; Reply.Data = MoveTemp(EncodedData.Bytes);
		TArray<uint8> Payload;
		if (!FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Result.Error)) return Result;
		auto Output = VoxelBoundedAllocation::MakeWorkerShared<FVoxelServerRepresentationResult>(CompletionLease);
		Output->Lease = CompletionLease;
		if (!FVoxelNetworkCodec::Encode(EVoxelMessage::RepresentationReply, Session, Payload, Output->Envelope))
		{ Result.Error = TEXT("Server representation envelope encoding failed"); return Result; }
		Output->Crc = FCrc::MemCrc32(Output->Envelope.GetData(), Output->Envelope.Num());
		Reply.Data.Empty(); Payload.Empty();
		EncodedData.ResourceLease.Reset();
		verify(CompletionLease->Resize(Output->GetAllocatedBytes()));
		Result.CustomPayload = MoveTemp(Output); Result.InputResourceLease = MetadataLease; Result.bSuccess = true;
		return Result;
	}
};
