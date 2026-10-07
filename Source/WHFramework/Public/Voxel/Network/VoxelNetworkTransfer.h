#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Task/VoxelTerrainResources.h"

class WHFRAMEWORK_API FVoxelNetworkTransfer
{
public:
	explicit FVoxelNetworkTransfer(const FVoxelNetworkSettings& InSettings = FVoxelNetworkSettings());

	bool BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Enqueue(EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease = nullptr);
	// Worker 已完成 Envelope 编码与分段 CRC；GT 只接管有界正文和原租约。
	bool EnqueuePrepared(EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
		uint32 InCrc, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease);
	void Tick(double InNow, TFunctionRef<void(const TArray<uint8>&)> InSend, double InDeadline = MAX_dbl);
	bool Receive(TConstArrayView<uint8> InPacket, double InNow, TArray<uint8>& OutCompleted,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease = nullptr);
	// GT 只拼装已限长的片段；完整正文的 CRC 和 Envelope 由后台任务验证。
	bool Assemble(TConstArrayView<uint8> InPacket, double InNow, TArray<uint8>& OutCompleted,
		uint32& OutCrc, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease = nullptr);
	void Reset();
	void SetSettings(const FVoxelNetworkSettings& InSettings);
	uint64 QueuedBytes() const;

private:
	struct FSend
	{
		uint64 Id = 0;
		uint64 Serial = 0;
		EVoxelTransferPriority Priority = EVoxelTransferPriority::None;
		TArray<uint8> Bytes;
		uint32 Crc = 0;
		int32 Offset = 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	};

	struct FReceive
	{
		int32 Total = 0;
		uint32 Crc = 0;
		TArray<uint8> Bytes;
		TBitArray<> Received;
		int32 Count = 0;
		double Since = 0.0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	};

	int32 FindNextSend() const;

private:
	FVoxelNetworkSettings Settings;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FVoxelGenerationConsumerKey Consumer;
	TArray<FSend> Pending;
	TMap<uint64, FReceive> Receiving;
	uint64 NextId = 1;
	uint64 NextSerial = 1;
	uint64 PendingBytes = 0;
	int32 ReceivingBytes = 0;
	double LastTick = 0.0;
	double Tokens = 0.0;
};
