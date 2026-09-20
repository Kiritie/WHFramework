#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"

class WHFRAMEWORK_API FVoxelNetworkTransfer
{
public:
	explicit FVoxelNetworkTransfer(const FVoxelNetworkSettings& InSettings = FVoxelNetworkSettings());

	bool Enqueue(EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage);
	void Tick(double InNow, TFunctionRef<void(const TArray<uint8>&)> InSend);
	bool Receive(TConstArrayView<uint8> InPacket, double InNow, TArray<uint8>& OutCompleted);
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
	};

	struct FReceive
	{
		int32 Total = 0;
		uint32 Crc = 0;
		TArray<uint8> Bytes;
		TBitArray<> Received;
		int32 Count = 0;
		double Since = 0.0;
	};

	int32 FindNextSend() const;

private:
	FVoxelNetworkSettings Settings;
	TArray<FSend> Pending;
	TMap<uint64, FReceive> Receiving;
	uint64 NextId = 1;
	uint64 NextSerial = 1;
	uint64 PendingBytes = 0;
	int32 ReceivingBytes = 0;
	double LastTick = 0.0;
	double Tokens = 0.0;
};
