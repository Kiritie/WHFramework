#include "Voxel/Network/VoxelNetworkTransfer.h"

#include "Misc/Crc.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	constexpr int32 FragmentBytes = 4096;
	constexpr int32 FragmentHeaderBytes = 28;
	constexpr int32 MaxPacketBytes = 4608;
	constexpr uint32 FragmentMagic = 0x34475246;
	constexpr int32 MaximumPendingTransfers = 256;
	constexpr int32 MaximumReceivingTransfers = 4;
}

FVoxelNetworkTransfer::FVoxelNetworkTransfer(const FVoxelNetworkSettings& InSettings)
	: Settings(InSettings)
	, Tokens(InSettings.BurstBytes)
{
}

FVoxelNetworkTransfer::~FVoxelNetworkTransfer()
{
	Reset();
}

FVoxelNetworkTransfer::FSend& FVoxelNetworkTransfer::FSend::operator=(FSend&& InOther)
{
	if (this == &InOther) return *this;
	Bytes = MoveTemp(InOther.Bytes);
	ResourceLease = MoveTemp(InOther.ResourceLease);
	Id = InOther.Id;
	Serial = InOther.Serial;
	Priority = InOther.Priority;
	Crc = InOther.Crc;
	Offset = InOther.Offset;
	return *this;
}

FVoxelNetworkTransfer::FReceive& FVoxelNetworkTransfer::FReceive::operator=(FReceive&& InOther)
{
	if (this == &InOther) return *this;
	Bytes = MoveTemp(InOther.Bytes);
	Received = MoveTemp(InOther.Received);
	ResourceLease = MoveTemp(InOther.ResourceLease);
	Total = InOther.Total;
	Crc = InOther.Crc;
	Count = InOther.Count;
	Since = InOther.Since;
	return *this;
}

bool FVoxelNetworkTransfer::Reject(const EVoxelTerrainResourceAdmission InAdmission,
	const TCHAR* InError, FString* OutError)
{
	LastResourceAdmission = InAdmission;
	if (OutError) *OutError = InError;
	return false;
}

bool FVoxelNetworkTransfer::Grant(const EVoxelTerrainResourcePool InPool, const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString* OutError)
{
	if (!Resources || !Consumer.IsValid())
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: original transfer ledger is not bound"), OutError);
	FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(), MetadataLease.Get());
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error))
		return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable, TEXT("Oversized: original transfer source floor"), OutError);
	LastResourceAdmission = Resources->TryAcquire(Consumer, InPool, InBytes, OutLease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (LastResourceAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		return Reject(LastResourceAdmission, LastResourceAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: original transfer native backing") :
			LastResourceAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: original transfer native backing") : TEXT("InvalidResource: original transfer consumer"), OutError);
	}
	if (OutError) OutError->Reset();
	return true;
}

bool FVoxelNetworkTransfer::Enqueue(const EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease, FString* OutError)
{
	if (InMessage.IsEmpty() || InMessage.Num() > FVoxelNetworkCodec::MaxWireBytes)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer wire length"), OutError);
	const uint32 Crc = FCrc::MemCrc32(InMessage.GetData(), InMessage.Num());
	return EnqueuePrepared(InPriority, MoveTemp(InMessage), Crc, InLease, OutError);
}

bool FVoxelNetworkTransfer::EnqueuePrepared(const EVoxelTransferPriority InPriority, TArray<uint8>&& InMessage,
	const uint32 InCrc, const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease, FString* OutError)
{
	if (!Resources || !MetadataLease || !InLease || !InLease->IsOwnedBy(*Resources) ||
		InLease->GetPool() != EVoxelTerrainResourcePool::Staged ||
		VoxelGenerationAllocation::AllocatedBytes(InMessage) > InLease->Bytes() ||
		InPriority == EVoxelTransferPriority::None || InPriority > EVoxelTransferPriority::Bulk ||
		InMessage.IsEmpty() || InMessage.Num() > FVoxelNetworkCodec::MaxWireBytes)
	{
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer requires the original pregranted staged body"), OutError);
	}
	if (NextId == MAX_uint64 || NextSerial == MAX_uint64 || Settings.MaxQueuedBytes <= 0 ||
		uint64(InMessage.Num()) > uint64(Settings.MaxQueuedBytes))
	{
		return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable, TEXT("Oversized: transfer identity or body limit"), OutError);
	}
	if (Pending.Num() >= MaximumPendingTransfers ||
		PendingBytes > uint64(Settings.MaxQueuedBytes) - uint64(InMessage.Num()))
	{
		return Reject(EVoxelTerrainResourceAdmission::None, TEXT("PlanningPending: original outbound transfer queue"), OutError);
	}
	if (Pending.Num() >= Pending.Max())
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer directory reserve contract"), OutError);
	FSend Send;
	Send.ResourceLease = InLease;
	Send.Id = NextId++;
	Send.Serial = NextSerial++;
	Send.Priority = InPriority;
	Send.Crc = InCrc;
	Send.Bytes = MoveTemp(InMessage);
	PendingBytes += Send.Bytes.Num();
	Pending.Add(MoveTemp(Send));
	Pending.Last().ResourceLease->MarkPublished();
	LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
	if (OutError) OutError->Reset();
	return true;
}

void FVoxelNetworkTransfer::Tick(const double InNow,
	TFunctionRef<void(TArray<uint8>&&)> InSend, const double InDeadline)
{
	if (!Resources || !MetadataLease) return;
	const auto OriginalResources = Resources;
	const auto OriginalConsumer = Consumer;
	if (LastTick == 0.0) LastTick = InNow;
	Tokens = FMath::Min(static_cast<double>(Settings.BurstBytes),
		Tokens + FMath::Clamp(InNow - LastTick, 0.0, 1.0) * Settings.BytesPerSecond);
	LastTick = InNow;
	for (int32 Count = 0; Count < 4 && !Pending.IsEmpty(); ++Count)
	{
		if (FPlatformTime::Seconds() >= InDeadline) break;
		const int32 SendIndex = FindNextSend();
		if (SendIndex == INDEX_NONE) break;
		const int32 PayloadBytes = FMath::Min(FragmentBytes, Pending[SendIndex].Bytes.Num() - Pending[SendIndex].Offset);
		const int32 PacketBytes = FragmentHeaderBytes + PayloadBytes;
		if (Tokens < PacketBytes) break;
		const auto BodyLease = Pending[SendIndex].ResourceLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		FVoxelGenerationRequiredSourceScope BodySource(Resources.Get(), BodyLease.Get());
		if (!Grant(EVoxelTerrainResourcePool::Working,
			VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(PacketBytes), Scratch, nullptr)) break;
		FVoxelByteWriter Writer(MaxPacketBytes);
		if (!Writer.Reserve(PacketBytes)) break;
		Writer.U32(FragmentMagic);
		Writer.U64(Pending[SendIndex].Id);
		Writer.U16(static_cast<uint16>(Pending[SendIndex].Offset / FragmentBytes));
		Writer.U16(static_cast<uint16>((Pending[SendIndex].Bytes.Num() + FragmentBytes - 1) / FragmentBytes));
		Writer.U32(static_cast<uint32>(Pending[SendIndex].Bytes.Num()));
		Writer.U32(Pending[SendIndex].Crc);
		Writer.Blob(MakeArrayView(Pending[SendIndex].Bytes).Slice(Pending[SendIndex].Offset, PayloadBytes), FragmentBytes);
		TArray<uint8> Packet;
		if (!Writer.Finish(Packet)) break;
		check(VoxelGenerationAllocation::AllocatedBytes(Packet) == Scratch->Bytes());
		Tokens -= Packet.Num();
		Pending[SendIndex].Offset += PayloadBytes;
		if (Pending[SendIndex].Offset == Pending[SendIndex].Bytes.Num())
		{
			PendingBytes -= Pending[SendIndex].Bytes.Num();
			Pending.RemoveAt(SendIndex, 1, EAllowShrinking::No);
		}
		InSend(MoveTemp(Packet));
		if (Resources != OriginalResources || Consumer != OriginalConsumer) break;
	}
	if (Resources != OriginalResources || Consumer != OriginalConsumer) return;
	for (auto Iterator = Receiving.CreateIterator(); Iterator; ++Iterator)
	{
		if (InNow - Iterator.Value().Since > 15.0)
		{
			ReceivingBytes -= Iterator.Value().Total;
			Iterator.RemoveCurrent();
			bReceiveGapPending = true;
		}
	}
}

bool FVoxelNetworkTransfer::Receive(const TConstArrayView<uint8> InPacket, const double InNow,
	TArray<uint8>& OutCompleted, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease, FString* OutError)
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompletedLease;
	TArray<uint8> Completed;
	uint32 Crc = 0;
	if (!Assemble(InPacket, InNow, Completed, Crc, &CompletedLease, OutError)) return false;
	if (!Completed.IsEmpty() && FCrc::MemCrc32(Completed.GetData(), Completed.Num()) != Crc)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer CRC mismatch"), OutError);
	if (!Completed.IsEmpty() && !OutLease)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: completed transfer requires its output lease"), OutError);
	OutCompleted = MoveTemp(Completed);
	if (OutLease) *OutLease = MoveTemp(CompletedLease);
	return true;
}

bool FVoxelNetworkTransfer::Assemble(const TConstArrayView<uint8> InPacket, const double InNow,
	TArray<uint8>& OutCompleted, uint32& OutCrc,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutLease, FString* OutError)
{
	if (!Resources || !MetadataLease || !OutLease)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer input domain or output lease"), OutError);
	if (InPacket.Num() > MaxPacketBytes)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer fragment length"), OutError);
	FVoxelByteReader Reader(InPacket);
	if (Reader.U32() != FragmentMagic)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer fragment magic"), OutError);
	const uint64 Id = Reader.U64();
	const uint16 Index = Reader.U16();
	const uint16 FragmentCount = Reader.U16();
	const uint32 Total = Reader.U32();
	const uint32 Crc = Reader.U32();
	const auto Data = Reader.BlobView(FragmentBytes);
	if (!Reader.End() || Id == 0 || Total == 0 || Total > static_cast<uint32>(FVoxelNetworkCodec::MaxWireBytes) ||
		FragmentCount != (Total + FragmentBytes - 1) / FragmentBytes || Index >= FragmentCount ||
		Data.Num() != FMath::Min<uint32>(FragmentBytes, Total - static_cast<uint32>(Index) * FragmentBytes))
	{
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: malformed transfer fragment"), OutError);
	}
	FReceive* ReceiveState = Receiving.Find(Id);
	if (!ReceiveState)
	{
		if (Settings.MaxQueuedBytes <= 0 || Total > uint32(Settings.MaxQueuedBytes))
			return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable, TEXT("Oversized: transfer receive body limit"), OutError);
		if (Receiving.Num() >= MaximumReceivingTransfers ||
			uint64(ReceivingBytes) + Total > uint64(Settings.MaxQueuedBytes))
			return Reject(EVoxelTerrainResourceAdmission::None, TEXT("PlanningPending: original receive transfer queue"), OutError);
		if (!VoxelGenerationAllocation::Private::SetCanReserve<TPair<uint64, FReceive>>(Receiving, Receiving.Num() + 1))
			return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: receive directory reserve contract"), OutError);
		FReceive NewState;
		const uint64 Words = (uint64(FragmentCount) + 31) / 32;
		const uint64 Flags = Words > 4 ? FMemory::QuantizeSize(Words * sizeof(uint32), DEFAULT_ALIGNMENT) : 0;
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(int32(Total)) + Flags;
		if (!Grant(EVoxelTerrainResourcePool::Input, Bytes, NewState.ResourceLease, OutError)) return false;
		NewState.Total = static_cast<int32>(Total);
		NewState.Crc = Crc;
		NewState.Bytes.Reserve(NewState.Total);
		NewState.Bytes.SetNumUninitialized(NewState.Total);
		NewState.Received.Init(false, FragmentCount);
		NewState.Since = InNow;
		check(VoxelGenerationAllocation::AllocatedBytes(NewState.Bytes) +
			(NewState.Received.GetAllocatedSize() > 4 * sizeof(uint32) ?
				FMemory::QuantizeSize(NewState.Received.GetAllocatedSize(), DEFAULT_ALIGNMENT) : 0) == NewState.ResourceLease->Bytes());
		ReceivingBytes += NewState.Total;
		ReceiveState = &Receiving.Add(Id, MoveTemp(NewState));
	}
	if (ReceiveState->Total != static_cast<int32>(Total) || ReceiveState->Crc != Crc ||
		ReceiveState->Received.Num() != FragmentCount)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer fragment identity changed"), OutError);
	const int32 Offset = static_cast<int32>(Index) * FragmentBytes;
	if (ReceiveState->Received[Index])
	{
		if (FMemory::Memcmp(ReceiveState->Bytes.GetData() + Offset, Data.GetData(), Data.Num()) != 0)
			return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: duplicate transfer fragment changed"), OutError);
	}
	else
	{
		FMemory::Memcpy(ReceiveState->Bytes.GetData() + Offset, Data.GetData(), Data.Num());
		ReceiveState->Received[Index] = true;
		++ReceiveState->Count;
		ReceiveState->Since = InNow;
	}
	if (ReceiveState->Count == FragmentCount)
	{
		ReceivingBytes -= ReceiveState->Total;
		ReceiveState->Received.Empty();
		const uint64 BodyBytes = VoxelGenerationAllocation::AllocatedBytes(ReceiveState->Bytes);
		if (ReceiveState->ResourceLease->TryResize(BodyBytes) != EVoxelTerrainResourceAdmission::Granted)
			return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer output settlement"), OutError);
		OutCrc = ReceiveState->Crc;
		OutCompleted = MoveTemp(ReceiveState->Bytes);
		*OutLease = MoveTemp(ReceiveState->ResourceLease);
		Receiving.Remove(Id);
	}
	else
	{
		OutCompleted.Empty();
		OutLease->Reset();
		OutCrc = 0;
	}
	LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
	if (OutError) OutError->Reset();
	return true;
}

void FVoxelNetworkTransfer::Reset()
{
	Pending.Empty();
	Receiving.Empty();
	MetadataLease.Reset();
	Resources.Reset();
	Consumer = {};
	LastResourceAdmission = EVoxelTerrainResourceAdmission::None;
	PendingBytes = 0;
	ReceivingBytes = 0;
	LastTick = 0.0;
	Tokens = Settings.BurstBytes;
	bReceiveGapPending = false;
}

bool FVoxelNetworkTransfer::ConsumeReceiveGap()
{
	const bool bPending = bReceiveGapPending;
	bReceiveGapPending = false;
	return bPending;
}

bool FVoxelNetworkTransfer::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (!InConsumer.IsValid() || (InResources->GetControlConsumer().IsValid() &&
		InConsumer.WorldEpoch != InResources->GetControlConsumer().WorldEpoch) ||
		(Resources && (Resources.Get() != &InResources.Get() || Consumer != InConsumer)))
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer binding domain changed"), nullptr);
	if (Resources)
	{
		LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	if (!Pending.IsEmpty() || !Receiving.IsEmpty() || VoxelGenerationAllocation::AllocatedBytes(Pending) != 0 ||
		VoxelGenerationAllocation::AllocatedBytes(Receiving) != 0)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: ungranted transfer cannot be retroactively bound"), nullptr);
	FString Error;
	if (!Settings.Validate(Error))
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer settings"), nullptr);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<FSend, FDefaultAllocator>(MaximumPendingTransfers) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<uint64, FReceive>>(Receiving, MaximumReceivingTransfers);
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Required, Error))
		return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable, TEXT("Oversized: transfer binding source floor"), nullptr);
	LastResourceAdmission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (LastResourceAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	Pending.Reserve(MaximumPendingTransfers);
	Receiving.Reserve(MaximumReceivingTransfers);
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Pending) + VoxelGenerationAllocation::AllocatedBytes(Receiving);
	check(ActualBytes <= Bytes);
	if (Lease->TryResize(ActualBytes) != EVoxelTerrainResourceAdmission::Granted)
	{
		Pending.Empty();
		Receiving.Empty();
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest, TEXT("InvalidResource: transfer directory settlement"), nullptr);
	}
	Resources = InResources;
	Consumer = InConsumer;
	MetadataLease = MoveTemp(Lease);
	MetadataLease->MarkPublished();
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
		if (Best == INDEX_NONE || Pending[Index].Priority < Pending[Best].Priority ||
			(Pending[Index].Priority == Pending[Best].Priority && Pending[Index].Serial < Pending[Best].Serial)) Best = Index;
	}
	return Best;
}
