#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Task/VoxelTerrainResources.h"

class WHFRAMEWORK_API FVoxelNetworkTransfer
{
public:
	explicit FVoxelNetworkTransfer(const FVoxelNetworkSettings& InSettings = FVoxelNetworkSettings());
	~FVoxelNetworkTransfer();
	FVoxelNetworkTransfer(const FVoxelNetworkTransfer&) = delete;
	FVoxelNetworkTransfer& operator=(const FVoxelNetworkTransfer&) = delete;

	bool BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return LastResourceAdmission; }
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GetResources() const { return Resources; }
	const FVoxelGenerationConsumerKey& GetConsumer() const { return Consumer; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GetMetadataLease() const { return MetadataLease; }
	bool Enqueue(EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease = nullptr, FString* OutError = nullptr);
	// Worker 已完成 Envelope 编码与分段 CRC；GT 只接管有界正文和原租约。
	bool EnqueuePrepared(EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
		uint32 InCrc, const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease, FString* OutError = nullptr);
	void Tick(double InNow, TFunctionRef<void(TArray<uint8>&&)> InSend, double InDeadline = MAX_dbl);
	// 完整输出必须同时接管原租约；调用方应先声明租约、后声明字节数组。
	bool Receive(TConstArrayView<uint8> InPacket, double InNow, TArray<uint8>& OutCompleted,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease = nullptr, FString* OutError = nullptr);
	// GT 只拼装已限长的片段；完整正文的 CRC 和 Envelope 由后台任务验证。
	bool Assemble(TConstArrayView<uint8> InPacket, double InNow, TArray<uint8>& OutCompleted,
		uint32& OutCrc, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease = nullptr, FString* OutError = nullptr);
	void Reset();
	bool ConsumeReceiveGap();
	void SetSettings(const FVoxelNetworkSettings& InSettings);
	uint64 QueuedBytes() const;

private:
	struct FSend
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		uint64 Id = 0;
		uint64 Serial = 0;
		EVoxelTransferPriority Priority = EVoxelTransferPriority::None;
		TArray<uint8> Bytes;
		uint32 Crc = 0;
		int32 Offset = 0;
		FSend() = default;
		FSend(FSend&&) = default;
		FSend& operator=(FSend&& InOther);
		FSend(const FSend&) = delete;
		FSend& operator=(const FSend&) = delete;
	};

	struct FReceive
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		int32 Total = 0;
		uint32 Crc = 0;
		TArray<uint8> Bytes;
		TBitArray<> Received;
		int32 Count = 0;
		double Since = 0.0;
		FReceive() = default;
		FReceive(FReceive&&) = default;
		FReceive& operator=(FReceive&& InOther);
		FReceive(const FReceive&) = delete;
		FReceive& operator=(const FReceive&) = delete;
	};

	int32 FindNextSend() const;
	bool Grant(EVoxelTerrainResourcePool InPool, uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString* OutError);
	bool Reject(EVoxelTerrainResourceAdmission InAdmission, const TCHAR* InError, FString* OutError);

private:
	FVoxelNetworkSettings Settings;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FVoxelGenerationConsumerKey Consumer;
	EVoxelTerrainResourceAdmission LastResourceAdmission = EVoxelTerrainResourceAdmission::None;
	TArray<FSend> Pending;
	TMap<uint64, FReceive> Receiving;
	uint64 NextId = 1;
	uint64 NextSerial = 1;
	uint64 PendingBytes = 0;
	int32 ReceivingBytes = 0;
	double LastTick = 0.0;
	double Tokens = 0.0;
	bool bReceiveGapPending = false;
};
