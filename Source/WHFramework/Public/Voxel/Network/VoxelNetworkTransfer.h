#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
class WHFRAMEWORK_API FVoxelNetworkTransfer
{
public:
	bool Enqueue(TArray<uint8>&& Message);
	void Tick(double Now, TFunctionRef<void(const TArray<uint8>&)> Send);
	bool Receive(TConstArrayView<uint8> Packet, double Now, TArray<uint8>& Completed);
	void Reset();
	uint64 QueuedBytes() const
	{
		return PendingBytes;
	}

private:
	struct FSend
	{
		uint64 ID = 0;
		TArray<uint8> Bytes;
		uint32 CRC = 0;
		int32 Offset = 0;
	};
	struct FReceive
	{
		int32 Total = 0;
		uint32 CRC = 0;
		TArray<uint8> Bytes;
		TBitArray<> Got;
		int32 Count = 0;
		double Since = 0;
	};
	TArray<FSend> Pending;
	TMap<uint64, FReceive> Receiving;
	uint64 NextID = 1;
	uint64 PendingBytes = 0;
	int32 ReceivingBytes = 0;
	double LastTick = 0;
	double Tokens = 32768;
};
