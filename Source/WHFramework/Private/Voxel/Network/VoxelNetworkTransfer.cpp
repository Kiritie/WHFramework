#include "Voxel/Network/VoxelNetworkTransfer.h"

#include "Misc/Crc.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace
{
	constexpr int32 FragmentBytes = 4096;
	constexpr int32 MaxPacketBytes = 4608;
	constexpr uint32 FragmentMagic = 0x34475246;
}

FVoxelNetworkTransfer::FVoxelNetworkTransfer(const FVoxelNetworkSettings& InSettings)
	: Settings(InSettings)
	, Tokens(InSettings.BurstBytes)
{
}

bool FVoxelNetworkTransfer::Enqueue(
	const EVoxelTransferPriority InPriority,
	TArray<uint8>&& InMessage)
{
	if (InPriority == EVoxelTransferPriority::None ||
		InMessage.IsEmpty() ||
		InMessage.Num() > FVoxelNetworkCodec::MaxWireBytes ||
		Pending.Num() >= 256 ||
		PendingBytes + InMessage.Num() > static_cast<uint64>(Settings.MaxQueuedBytes) ||
		NextId == MAX_uint64 ||
		NextSerial == MAX_uint64)
	{
		return false;
	}

	FSend Send;
	Send.Id = NextId++;
	Send.Serial = NextSerial++;
	Send.Priority = InPriority;
	Send.Crc = FCrc::MemCrc32(InMessage.GetData(), InMessage.Num());
	Send.Bytes = MoveTemp(InMessage);
	PendingBytes += Send.Bytes.Num();
	Pending.Add(MoveTemp(Send));
	return true;
}

void FVoxelNetworkTransfer::Tick(
	const double InNow,
	TFunctionRef<void(const TArray<uint8>&)> InSend)
{
	if (LastTick == 0.0)
	{
		LastTick = InNow;
	}
	Tokens = FMath::Min(
		static_cast<double>(Settings.BurstBytes),
		Tokens + FMath::Clamp(InNow - LastTick, 0.0, 1.0) * Settings.BytesPerSecond);
	LastTick = InNow;

	for (int32 Count = 0; Count < 4 && !Pending.IsEmpty(); ++Count)
	{
		const int32 SendIndex = FindNextSend();
		if (SendIndex == INDEX_NONE)
		{
			break;
		}
		FSend& Send = Pending[SendIndex];
		const int32 PayloadBytes = FMath::Min(FragmentBytes, Send.Bytes.Num() - Send.Offset);
		if (Tokens < PayloadBytes + 32)
		{
			break;
		}

		FVoxelByteWriter Writer(MaxPacketBytes);
		Writer.U32(FragmentMagic);
		Writer.U64(Send.Id);
		Writer.U16(static_cast<uint16>(Send.Offset / FragmentBytes));
		Writer.U16(static_cast<uint16>((Send.Bytes.Num() + FragmentBytes - 1) / FragmentBytes));
		Writer.U32(static_cast<uint32>(Send.Bytes.Num()));
		Writer.U32(Send.Crc);
		Writer.Blob(MakeArrayView(Send.Bytes).Slice(Send.Offset, PayloadBytes), FragmentBytes);
		TArray<uint8> Packet;
		if (!Writer.Finish(Packet))
		{
			break;
		}
		InSend(Packet);
		Tokens -= Packet.Num();
		Send.Offset += PayloadBytes;
		if (Send.Offset == Send.Bytes.Num())
		{
			PendingBytes -= Send.Bytes.Num();
			Pending.RemoveAt(SendIndex, 1, EAllowShrinking::No);
		}
	}

	for (auto Iterator = Receiving.CreateIterator(); Iterator; ++Iterator)
	{
		if (InNow - Iterator.Value().Since > 15.0)
		{
			ReceivingBytes -= Iterator.Value().Total;
			Iterator.RemoveCurrent();
		}
	}
}

bool FVoxelNetworkTransfer::Receive(
	TConstArrayView<uint8> InPacket,
	const double InNow,
	TArray<uint8>& OutCompleted)
{
	OutCompleted.Reset();
	if (InPacket.Num() > MaxPacketBytes)
	{
		return false;
	}

	FVoxelByteReader Reader(InPacket);
	if (Reader.U32() != FragmentMagic)
	{
		return false;
	}
	const uint64 Id = Reader.U64();
	const uint16 Index = Reader.U16();
	const uint16 FragmentCount = Reader.U16();
	const uint32 Total = Reader.U32();
	const uint32 Crc = Reader.U32();
	const TArray<uint8> Data = Reader.Blob(FragmentBytes);
	if (!Reader.End() ||
		Id == 0 ||
		Total == 0 ||
		Total > static_cast<uint32>(FVoxelNetworkCodec::MaxWireBytes) ||
		FragmentCount != (Total + FragmentBytes - 1) / FragmentBytes ||
		Index >= FragmentCount ||
		Data.Num() != FMath::Min<uint32>(FragmentBytes, Total - static_cast<uint32>(Index) * FragmentBytes))
	{
		return false;
	}

	FReceive* ReceiveState = Receiving.Find(Id);
	if (!ReceiveState)
	{
		if (Receiving.Num() >= 4 ||
			ReceivingBytes + static_cast<int32>(Total) > Settings.MaxQueuedBytes)
		{
			return false;
		}
		FReceive NewState;
		NewState.Total = static_cast<int32>(Total);
		NewState.Crc = Crc;
		NewState.Bytes.SetNumUninitialized(NewState.Total);
		NewState.Received.Init(false, FragmentCount);
		NewState.Since = InNow;
		ReceivingBytes += NewState.Total;
		ReceiveState = &Receiving.Add(Id, MoveTemp(NewState));
	}
	if (ReceiveState->Total != static_cast<int32>(Total) ||
		ReceiveState->Crc != Crc ||
		ReceiveState->Received.Num() != FragmentCount)
	{
		return false;
	}

	const int32 Offset = static_cast<int32>(Index) * FragmentBytes;
	if (ReceiveState->Received[Index])
	{
		return FMemory::Memcmp(
			ReceiveState->Bytes.GetData() + Offset,
			Data.GetData(),
			Data.Num()) == 0;
	}
	FMemory::Memcpy(ReceiveState->Bytes.GetData() + Offset, Data.GetData(), Data.Num());
	ReceiveState->Received[Index] = true;
	++ReceiveState->Count;
	if (ReceiveState->Count == FragmentCount)
	{
		const bool bValid = FCrc::MemCrc32(
			ReceiveState->Bytes.GetData(),
			ReceiveState->Bytes.Num()) == ReceiveState->Crc;
		ReceivingBytes -= ReceiveState->Total;
		if (bValid)
		{
			OutCompleted = MoveTemp(ReceiveState->Bytes);
		}
		Receiving.Remove(Id);
		return bValid;
	}
	return true;
}

void FVoxelNetworkTransfer::Reset()
{
	Pending.Reset();
	Receiving.Reset();
	PendingBytes = 0;
	ReceivingBytes = 0;
	LastTick = 0.0;
	Tokens = Settings.BurstBytes;
}

void FVoxelNetworkTransfer::SetSettings(const FVoxelNetworkSettings& InSettings)
{
	Settings = InSettings;
	Tokens = FMath::Min(Tokens, static_cast<double>(Settings.BurstBytes));
}

uint64 FVoxelNetworkTransfer::QueuedBytes() const
{
	return PendingBytes;
}

int32 FVoxelNetworkTransfer::FindNextSend() const
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		if (Best == INDEX_NONE ||
			static_cast<uint8>(Pending[Index].Priority) < static_cast<uint8>(Pending[Best].Priority) ||
			(Pending[Index].Priority == Pending[Best].Priority && Pending[Index].Serial < Pending[Best].Serial))
		{
			Best = Index;
		}
	}
	return Best;
}
