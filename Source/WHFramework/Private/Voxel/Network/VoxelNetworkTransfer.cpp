#include "Voxel/Network/VoxelNetworkTransfer.h"

#include "Misc/Crc.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

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
	TArray<uint8>&& InMessage,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
{
	if (InMessage.IsEmpty() || InMessage.Num() > FVoxelNetworkCodec::MaxWireBytes) return false;
	const uint32 Crc = FCrc::MemCrc32(InMessage.GetData(), InMessage.Num());
	return EnqueuePrepared(InPriority, MoveTemp(InMessage), Crc, MoveTemp(InLease));
}

bool FVoxelNetworkTransfer::EnqueuePrepared(
	const EVoxelTransferPriority InPriority,
	TArray<uint8>&& InMessage,
	const uint32 InCrc,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
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
	if (Resources && !InLease)
	{
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, InMessage.GetAllocatedSize(), InLease)) return false;
	}
	Send.ResourceLease = MoveTemp(InLease);
	Send.Id = NextId++;
	Send.Serial = NextSerial++;
	Send.Priority = InPriority;
	Send.Crc = InCrc;
	Send.Bytes = MoveTemp(InMessage);
	PendingBytes += Send.Bytes.Num();
	Pending.Add(MoveTemp(Send));
	return true;
}

void FVoxelNetworkTransfer::Tick(
	const double InNow,
	TFunctionRef<void(const TArray<uint8>&)> InSend,
	const double InDeadline)
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
		if (FPlatformTime::Seconds() >= InDeadline) break;
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

		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		if (Resources && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::InitialGrowBytes<uint8>(MaxPacketBytes) * 3, Scratch)) break;
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
	TArray<uint8>& OutCompleted,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease)
{
	uint32 Crc = 0;
	if (!Assemble(InPacket, InNow, OutCompleted, Crc, OutLease)) return false;
	if (OutCompleted.IsEmpty() || FCrc::MemCrc32(OutCompleted.GetData(), OutCompleted.Num()) == Crc) return true;
	OutCompleted.Reset();
	if (OutLease) OutLease->Reset();
	return false;
}

bool FVoxelNetworkTransfer::Assemble(TConstArrayView<uint8> InPacket, const double InNow,
	TArray<uint8>& OutCompleted, uint32& OutCrc,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease)
{
	OutCompleted.Reset();
	OutCrc = 0;
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
	const auto Data = Reader.BlobView(FragmentBytes);
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
		if (Resources)
		{
			const uint64 Words = (uint64(FragmentCount) + 31) / 32;
			const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint8>(Total) +
				(Words > 4 ? VoxelBoundedAllocation::ArrayBytes<uint32>(Words) : 0);
			if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, NewState.ResourceLease)) return false;
		}
		NewState.Total = static_cast<int32>(Total);
		NewState.Crc = Crc;
		NewState.Bytes.Reserve(NewState.Total);
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
		ReceivingBytes -= ReceiveState->Total;
		OutCrc = ReceiveState->Crc;
		OutCompleted = MoveTemp(ReceiveState->Bytes);
		if (OutLease) *OutLease = MoveTemp(ReceiveState->ResourceLease);
		Receiving.Remove(Id);
		return true;
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

bool FVoxelNetworkTransfer::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (Resources.Get() == &InResources.Get() && Consumer.WorldEpoch == InConsumer.WorldEpoch &&
		Consumer.OwnerId == InConsumer.OwnerId && Consumer.OwnerIncarnation == InConsumer.OwnerIncarnation) return true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FSend>(256) +
		VoxelBoundedAllocation::MapBytes<uint64, FReceive>(4);
	if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident, Bytes, Lease)) return false;
	for (auto& PendingSend : Pending)
		if (!PendingSend.ResourceLease && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Staged,
			PendingSend.Bytes.GetAllocatedSize(), PendingSend.ResourceLease)) return false;
	for (auto& PendingReceive : Receiving)
		if (!PendingReceive.Value.ResourceLease && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			PendingReceive.Value.Bytes.GetAllocatedSize() + PendingReceive.Value.Received.GetAllocatedSize(), PendingReceive.Value.ResourceLease)) return false;
	Resources = InResources;
	Consumer = InConsumer;
	MetadataLease = MoveTemp(Lease);
	Pending.Reserve(256);
	Receiving.Reserve(4);
	return true;
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
