#include "Voxel/Network/VoxelOverlayReplica.h"

#include "HAL/PlatformMisc.h"
#include "Async/Async.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

namespace
{
	constexpr uint64 MaximumReceivingBytes = 32ull * 1024 * 1024;
	bool LessKey(const FIntVector& A, const FIntVector& B)
	{
		if (A.X != B.X) return A.X < B.X;
		if (A.Y != B.Y) return A.Y < B.Y;
		return A.Z < B.Z;
	}
	template<typename KeyType, typename ValueType>
	bool EqualMap(const TMap<KeyType, ValueType>& A, const TMap<KeyType, ValueType>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (const auto& Pair : A)
		{
			const auto* Value = B.Find(Pair.Key);
			if (!Value || !(*Value == Pair.Value)) return false;
		}
		return true;
	}
	void WriteHash(FVoxelByteWriter& Writer, const FVoxelOverlayHash& Hash)
	{
		Writer.Raw(MakeArrayView(Hash.Bytes));
	}
	FVoxelOverlayHash ReadHash(FVoxelByteReader& Reader)
	{
		FVoxelOverlayHash Hash;
		const auto Bytes = Reader.View(32);
		if (Bytes.Num() == 32) FMemory::Memcpy(Hash.Bytes, Bytes.GetData(), 32);
		return Hash;
	}
	void WriteEntry(FVoxelByteWriter& Writer, const FVoxelOverlayDirectoryEntry& Entry, const bool bDelta)
	{
		FVoxelNetworkCodec::WriteVector(Writer, Entry.Section);
		Writer.U64(Entry.Revision);
		Writer.U8(Entry.bTombstone ? 1 : 0);
		Writer.U32(Entry.PayloadBytes);
		WriteHash(Writer, Entry.PayloadHash);
		if (bDelta)
		{
			Writer.U64(Entry.BeforeRevision);
			WriteHash(Writer, Entry.BeforeHash);
		}
	}
	bool ReadEntry(FVoxelByteReader& Reader, FVoxelOverlayDirectoryEntry& Entry, const bool bDelta)
	{
		Entry.Section = FVoxelNetworkCodec::ReadVector(Reader);
		Entry.Revision = Reader.U64();
		const uint8 Tombstone = Reader.U8();
		Entry.bTombstone = Tombstone == 1;
		Entry.PayloadBytes = Reader.U32();
		Entry.PayloadHash = ReadHash(Reader);
		if (bDelta)
		{
			Entry.BeforeRevision = Reader.U64();
			Entry.BeforeHash = ReadHash(Reader);
		}
		return Reader.IsValid() && Tombstone <= 1 && Entry.Revision > 0 &&
			Entry.PayloadBytes <= FVoxelNetworkCodec::MaxRepresentationBytes &&
			(Entry.bTombstone ? Entry.PayloadBytes == 0 : Entry.PayloadBytes > 0);
	}
	bool IsRegionMessage(const EVoxelMessage Kind)
	{
		return Kind >= EVoxelMessage::SubscribeRegion && Kind <= EVoxelMessage::ResyncRequest;
	}
	bool HashEntries(TConstArrayView<FVoxelOverlayDirectoryEntry> Entries, const bool bSnapshot, FVoxelOverlayHash& OutHash)
	{
		FVoxelByteWriter Writer(FVoxelOverlayRegionCodec::MaximumSnapshotBytes);
		Writer.U32(Entries.Num());
		FIntVector Previous;
		bool bPrevious = false;
		for (const auto& Entry : Entries)
		{
			if (bPrevious && !LessKey(Previous, Entry.Section)) return false;
			WriteEntry(Writer, Entry, false);
			if (bSnapshot) WriteHash(Writer, Entry.PayloadHash);
			Previous = Entry.Section;
			bPrevious = true;
		}
		TArray<uint8> Bytes;
		return Writer.Finish(Bytes) && FVoxelOverlayHash::Compute(Bytes, OutHash);
	}
	uint8 Branch(const FIntVector& Key, const int32 Bit)
	{
		return (((uint32(Key.X) ^ 0x80000000u) >> Bit) & 1u) |
			((((uint32(Key.Y) ^ 0x80000000u) >> Bit) & 1u) << 1) |
			((((uint32(Key.Z) ^ 0x80000000u) >> Bit) & 1u) << 2);
	}
}

bool FVoxelOverlayHash::operator==(const FVoxelOverlayHash& InOther) const
{
	return FMemory::Memcmp(Bytes, InOther.Bytes, 32) == 0;
}

bool FVoxelOverlayHash::operator!=(const FVoxelOverlayHash& InOther) const
{
	return !(*this == InOther);
}

bool FVoxelOverlayHash::IsZero() const
{
	return *this == FVoxelOverlayHash();
}

bool FVoxelOverlayHash::Compute(TConstArrayView<uint8> InBytes, FVoxelOverlayHash& OutHash)
{
	FSHA256Signature Signature;
	if (!FPlatformMisc::GetSHA256Signature(InBytes.GetData(), InBytes.Num(), Signature)) return false;
	FMemory::Memcpy(OutHash.Bytes, Signature.Signature, 32);
	return true;
}

uint64 FVoxelOverlayRegionMessage::AllocatedBytes() const
{
	return sizeof(*this) + Heads.GetAllocatedSize() + Entries.GetAllocatedSize() + Payload.GetAllocatedSize();
}

FIntVector FVoxelOverlayRegionCodec::RegionOf(const FIntVector& InSection)
{
	return FIntVector(VoxelGeneration::FloorDivide(InSection.X, 8),
		VoxelGeneration::FloorDivide(InSection.Y, 8), VoxelGeneration::FloorDivide(InSection.Z, 8));
}

bool FVoxelOverlayRegionCodec::GatherRegions(const FVoxelGenerationBounds& InBounds, TArray<FIntVector>& OutRegions)
{
	OutRegions.Reset();
	if (!InBounds.IsValid()) return false;
	const FIntVector Min(VoxelGeneration::FloorDivide(InBounds.Min.X, 128),
		VoxelGeneration::FloorDivide(InBounds.Min.Y, 128), VoxelGeneration::FloorDivide(InBounds.Min.Z, 128));
	const FIntVector Max(VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 128),
		VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 128), VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 128));
	const FIntVector Size = Max - Min + FIntVector(1);
	if (Size.GetMin() <= 0 || Size.GetMax() > MaximumRegions || int64(Size.X) * Size.Y * Size.Z > MaximumRegions) return false;
	for (int32 X = Min.X; X <= Max.X; ++X)
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			for (int32 Z = Min.Z; Z <= Max.Z; ++Z) OutRegions.Add(FIntVector(X, Y, Z));
	return true;
}

bool FVoxelOverlayRegionCodec::ManifestHash(const FVoxelWorldManifest& InManifest, FVoxelOverlayHash& OutHash)
{
	TArray<uint8> Bytes;
	return FVoxelManifestCodec::Encode(InManifest, Bytes) && FVoxelOverlayHash::Compute(Bytes, OutHash);
}

bool FVoxelOverlayRegionCodec::Encode(const EVoxelMessage InKind, const FVoxelOverlayRegionMessage& M, TArray<uint8>& OutBytes)
{
	if (!IsRegionMessage(InKind) || M.WorldEpoch == 0 || M.ManifestIdentity.IsZero() || !M.SubscriptionId.IsValid() ||
		!M.CoverageTicket.IsValid() || M.Heads.Num() > MaximumRegions || M.Entries.Num() > MaximumPageEntries ||
		M.Payload.Num() > MaximumPartBytes || M.TotalBytes > MaximumSnapshotBytes) return false;
	FVoxelByteWriter Writer(MaximumMessageBytes);
	Writer.U8(uint8(InKind));
	Writer.U64(M.WorldEpoch);
	WriteHash(Writer, M.ManifestIdentity);
	Writer.Guid(M.SubscriptionId); Writer.Guid(M.CoverageTicket); Writer.Guid(M.DirectoryEpoch);
	Writer.Guid(M.SnapshotId); Writer.Guid(M.AuthorityTransaction);
	FVoxelNetworkCodec::WriteVector(Writer, M.Region); FVoxelNetworkCodec::WriteVector(Writer, M.Section);
	Writer.U64(M.AuthorityCut); Writer.U64(M.DirectoryRevision); Writer.U64(M.BaseSequence);
	Writer.U64(M.TargetSequence); Writer.U64(M.OverlayRevision);
	Writer.U32(M.Index); Writer.U32(M.Count); Writer.U32(M.EntryCount); Writer.U32(M.TotalBytes);
	WriteHash(Writer, M.DirectoryHash); WriteHash(Writer, M.ContentHash);
	Writer.U16(M.Heads.Num());
	for (const auto& Head : M.Heads)
	{
		FVoxelNetworkCodec::WriteVector(Writer, Head.Region);
		Writer.U64(Head.PreviousSequence); Writer.U64(Head.Sequence);
		WriteHash(Writer, Head.BeforeHash); WriteHash(Writer, Head.Hash);
	}
	Writer.U8(M.Entries.Num());
	for (const auto& Entry : M.Entries) WriteEntry(Writer, Entry, true);
	Writer.Blob(M.Payload, MaximumPartBytes);
	TArray<uint8> Body;
	FVoxelOverlayHash Hash;
	if (!Writer.Finish(Body) || !FVoxelOverlayHash::Compute(Body, Hash)) return false;
	Body.Append(Hash.Bytes, 32);
	OutBytes = MoveTemp(Body);
	return true;
}

bool FVoxelOverlayRegionCodec::Decode(const EVoxelMessage InKind, TConstArrayView<uint8> InBytes,
	FVoxelOverlayRegionMessage& OutMessage, FString& OutError)
{
	if (!IsRegionMessage(InKind) || InBytes.Num() < 32 || InBytes.Num() > MaximumMessageBytes) return false;
	FVoxelOverlayHash Hash;
	if (!FVoxelOverlayHash::Compute(InBytes.Left(InBytes.Num() - 32), Hash) ||
		FMemory::Memcmp(Hash.Bytes, InBytes.GetData() + InBytes.Num() - 32, 32) != 0)
	{
		OutError = TEXT("区域消息 SHA256 不匹配");
		return false;
	}
	FVoxelByteReader Reader(InBytes.Left(InBytes.Num() - 32));
	if (Reader.U8() != uint8(InKind)) return false;
	FVoxelOverlayRegionMessage M;
	M.ResourceLease = OutMessage.ResourceLease;
	M.WireDigest = Hash;
	M.WorldEpoch = Reader.U64(); M.ManifestIdentity = ReadHash(Reader);
	M.SubscriptionId = Reader.Guid(); M.CoverageTicket = Reader.Guid(); M.DirectoryEpoch = Reader.Guid();
	M.SnapshotId = Reader.Guid(); M.AuthorityTransaction = Reader.Guid();
	M.Region = FVoxelNetworkCodec::ReadVector(Reader); M.Section = FVoxelNetworkCodec::ReadVector(Reader);
	M.AuthorityCut = Reader.U64(); M.DirectoryRevision = Reader.U64(); M.BaseSequence = Reader.U64();
	M.TargetSequence = Reader.U64(); M.OverlayRevision = Reader.U64();
	M.Index = Reader.U32(); M.Count = Reader.U32(); M.EntryCount = Reader.U32(); M.TotalBytes = Reader.U32();
	M.DirectoryHash = ReadHash(Reader); M.ContentHash = ReadHash(Reader);
	const uint16 HeadCount = Reader.U16();
	if (HeadCount > MaximumRegions || M.TotalBytes > MaximumSnapshotBytes) return false;
	M.Heads.Reserve(HeadCount);
	TSet<FIntVector> Regions;
	for (int32 Index = 0; Index < HeadCount; ++Index)
	{
		FVoxelOverlayRegionHead Head;
		Head.Region = FVoxelNetworkCodec::ReadVector(Reader);
		Head.PreviousSequence = Reader.U64(); Head.Sequence = Reader.U64();
		Head.BeforeHash = ReadHash(Reader); Head.Hash = ReadHash(Reader);
		if (Regions.Contains(Head.Region)) return false;
		Regions.Add(Head.Region); M.Heads.Add(Head);
	}
	const uint8 EntryCount = Reader.U8();
	if (EntryCount > MaximumPageEntries) return false;
	M.Entries.Reserve(EntryCount);
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		FVoxelOverlayDirectoryEntry Entry;
		if (!ReadEntry(Reader, Entry, true)) return false;
		M.Entries.Add(Entry);
	}
	M.Payload = Reader.Blob(MaximumPartBytes);
	if (!Reader.End() || M.WorldEpoch == 0 || M.ManifestIdentity.IsZero() || !M.SubscriptionId.IsValid() ||
		!M.CoverageTicket.IsValid()) return false;
	OutMessage = MoveTemp(M);
	OutError.Reset();
	return true;
}

bool FVoxelOverlayRegionCodec::DirectoryHash(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries, FVoxelOverlayHash& OutHash)
{
	return HashEntries(InEntries, false, OutHash);
}

bool FVoxelOverlayRegionCodec::SnapshotHash(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries, FVoxelOverlayHash& OutHash)
{
	return HashEntries(InEntries, true, OutHash);
}

bool FVoxelOverlayRegionCodec::EncodeOverlay(const FVoxelOverlaySnapshot& InOverlay, TArray<uint8>& OutBytes, FString& OutError)
{
	FVoxelNetworkSectionSnapshot Snapshot;
	Snapshot.Section = InOverlay.Section; Snapshot.Revision = InOverlay.Revision;
	for (const auto& Pair : InOverlay.Blocks) Snapshot.Blocks.Add({Pair.Key, false, Pair.Value});
	for (const auto& Pair : InOverlay.Entities) Snapshot.Entities.Add({Pair.Key, false, Pair.Value});
	Snapshot.Blocks.Sort([](const auto& A, const auto& B) { return A.CellIndex < B.CellIndex; });
	Snapshot.Entities.Sort([](const auto& A, const auto& B) { return A.CellIndex < B.CellIndex; });
	return FVoxelNetworkCodec::EncodeSectionSnapshot(Snapshot, OutBytes, OutError);
}

bool FVoxelOverlayRegionCodec::DecodeOverlay(TConstArrayView<uint8> InBytes, FVoxelOverlaySnapshot& OutOverlay, FString& OutError)
{
	uint64 Capacity;
	if (!OverlayCapacity(InBytes, Capacity))
	{
		OutError = TEXT("Overlay 规范正文不合法");
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	FVoxelOverlaySnapshot Out;
	Out.Section = FVoxelNetworkCodec::ReadVector(Reader); Out.Revision = Reader.U64();
	const uint16 Blocks = Reader.U16();
	Out.Blocks.Reserve(Blocks);
	for (uint16 Index = 0; Index < Blocks; ++Index)
	{
		const uint16 Cell = Reader.U16();
		Reader.U8();
		Out.Blocks.Add(Cell, FVoxelBlockState::Unpack(Reader.U32()));
	}
	const uint16 Entities = Reader.U16();
	Out.Entities.Reserve(Entities);
	for (uint16 Index = 0; Index < Entities; ++Index)
	{
		const uint16 Cell = Reader.U16();
		Reader.U8();
		FVoxelBlockEntityState Entity;
		Entity.Kind = Reader.U16(); Entity.Schema = Reader.U16();
		const auto Payload = Reader.BlobView(256 * 1024);
		Entity.Payload.Reserve(Payload.Num());
		Entity.Payload.Append(Payload.GetData(), Payload.Num());
		Out.Entities.Add(Cell, MoveTemp(Entity));
	}
	if (!Reader.End()) return false;
	OutOverlay = MoveTemp(Out);
	OutError.Reset();
	return true;
}

bool FVoxelOverlayRegionCodec::OverlayCapacity(TConstArrayView<uint8> InBytes, uint64& OutBytes)
{
	if (InBytes.Num() > FVoxelNetworkCodec::MaxRepresentationBytes) return false;
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkCodec::ReadVector(Reader);
	if (Reader.U64() == 0) return false;
	const uint16 Blocks = Reader.U16();
	if (Blocks > 4096) return false;
	int32 Previous = INDEX_NONE;
	for (uint16 Index = 0; Index < Blocks; ++Index)
	{
		const uint16 Cell = Reader.U16();
		if (Cell >= 4096 || Cell <= Previous || Reader.U8() != 0) return false;
		Reader.U32(); Previous = Cell;
	}
	const uint16 Entities = Reader.U16();
	if (Entities > 4096) return false;
	uint64 PayloadBytes = 0;
	Previous = INDEX_NONE;
	for (uint16 Index = 0; Index < Entities; ++Index)
	{
		const uint16 Cell = Reader.U16();
		if (Cell >= 4096 || Cell <= Previous || Reader.U8() != 0) return false;
		Reader.U16(); Reader.U16();
		const auto Payload = Reader.BlobView(256 * 1024);
		PayloadBytes += VoxelBoundedAllocation::ArrayBytes<uint8>(Payload.Num());
		Previous = Cell;
	}
	if (!Reader.End()) return false;
	OutBytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlaySnapshot>() +
		VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockState>(Blocks) +
		VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockEntityState>(Entities) + PayloadBytes;
	return true;
}

bool FVoxelOverlayRegionCodec::EncodeDelta(TConstArrayView<FVoxelOverlayDirectoryEntry> InEntries,
	TConstArrayView<TArray<uint8>> InPayloads, TArray<uint8>& OutBytes)
{
	if (InEntries.Num() != InPayloads.Num() || InEntries.Num() > 32768) return false;
	FVoxelByteWriter Writer(MaximumSnapshotBytes);
	Writer.U32(InEntries.Num());
	for (int32 Index = 0; Index < InEntries.Num(); ++Index)
	{
		WriteEntry(Writer, InEntries[Index], true);
		Writer.Blob(InPayloads[Index], FVoxelNetworkCodec::MaxRepresentationBytes);
	}
	return Writer.Finish(OutBytes);
}

bool FVoxelOverlayRegionCodec::DecodeDelta(TConstArrayView<uint8> InBytes,
	TArray<FVoxelOverlayDirectoryEntry>& OutEntries, TArray<TArray<uint8>>& OutPayloads)
{
	FVoxelByteReader Reader(InBytes);
	const uint32 Count = Reader.U32();
	if (Count > 32768) return false;
	TSet<FIntVector> Seen;
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		FVoxelOverlayDirectoryEntry Entry;
		if (!ReadEntry(Reader, Entry, true) || Seen.Contains(Entry.Section)) return false;
		Seen.Add(Entry.Section);
		OutEntries.Add(Entry);
		OutPayloads.Add(Reader.Blob(FVoxelNetworkCodec::MaxRepresentationBytes));
	}
	return Reader.End();
}

struct FVoxelOverlayReplicaRoot
{
	TSharedPtr<const FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe> Children[8];
	TSharedPtr<const FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe> Region;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

namespace
{
	using FRootPtr = TSharedPtr<const FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe>;
	using FRegionPtr = TSharedPtr<const FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe>;
	FRegionPtr RegionAt(const FRootPtr& Root, const FIntVector& Key)
	{
		const auto* Node = Root.Get();
		for (int32 Bit = 31; Bit >= 0 && Node; --Bit) Node = Node->Children[Branch(Key, Bit)].Get();
		return Node ? Node->Region : nullptr;
	}
	uint64 RootNodeBytes()
	{
		return VoxelBoundedAllocation::SharedBytes<FVoxelOverlayReplicaRoot>();
	}
	FRootPtr SetRegion(const FRootPtr& Base, const FRegionPtr& Region, const int32 Bit,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		auto NodeLease = Lease->Split(RootNodeBytes());
		check(NodeLease);
		auto Node = MakeShared<FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe>();
		Node->Lease = MoveTemp(NodeLease);
		if (Base)
		{
			for (int32 Index = 0; Index < 8; ++Index) Node->Children[Index] = Base->Children[Index];
			Node->Region = Base->Region;
		}
		if (Bit < 0) Node->Region = Region;
		else
		{
			const uint8 Index = Branch(Region->Region, Bit);
			Node->Children[Index] = SetRegion(Node->Children[Index], Region, Bit - 1, Lease);
		}
		return Node;
	}
	struct FRemoteReadLease final : IVoxelFineOverlayReadLeaseSet
	{
		uint64 Epoch = 0;
		FGuid DirectoryEpoch;
		FVoxelGenerationBounds Bounds;
		TArray<FRegionPtr> Regions;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		virtual uint64 GetWorldEpoch() const override { return Epoch; }
		virtual bool IsRemoteDirectory() const override { return true; }
		virtual uint64 GetMetadataBytes() const override { return sizeof(*this) + Regions.GetAllocatedSize(); }
		virtual bool ValidateCapture(const FVoxelFineOverlayCapture& Capture, FString& OutError) const override
		{
			if (Capture.WorldEpoch != Epoch || Capture.ReplicaDirectoryEpoch != DirectoryEpoch || !Capture.Reads.IsEmpty() ||
				Capture.ReplicaRegionSequences.Num() != Regions.Num() || Capture.Bounds.Min != Bounds.Min || Capture.Bounds.Max != Bounds.Max)
			{
				OutError = TEXT("Replica 捕获不属于同一个已验证目录根");
				return false;
			}
			int32 ModifiedCount = 0;
			for (const auto& Region : Regions)
			{
				const uint64* Sequence = Capture.ReplicaRegionSequences.Find(Region->Region);
				if (!Sequence || *Sequence != Region->Sequence) return false;
				for (const auto& Pair : Region->Overlays)
				{
					if (!Bounds.Intersects({Pair.Key * 16, (Pair.Key + FIntVector(1)) * 16})) continue;
					const auto* Overlay = Capture.Resident.Find(Pair.Key);
					const uint64* Revision = Capture.ContentRevisions.Find(Pair.Key);
					if (!Overlay || !Revision || *Revision != Pair.Value->Revision || Overlay->Revision != *Revision ||
						Overlay != Pair.Value.Get()) return false;
					++ModifiedCount;
				}
			}
			return ModifiedCount == Capture.Resident.Num() && ModifiedCount == Capture.ContentRevisions.Num();
		}
		virtual bool ValidateRead(const FVoxelRegionReadView&, TConstArrayView<uint8>, uint64, FString& OutError) const override
		{
			OutError = TEXT("客户端目录不接受任意本地文件正文");
			return false;
		}
	};
	struct FPartSet
	{
		uint64 StoredBytes = 0;
		uint32 TotalBytes = 0;
		uint32 Count = 0;
		FVoxelOverlayHash Hash;
		TSharedPtr<const FVoxelOverlayRegionMessage, ESPMode::ThreadSafe> Cohort;
		TMap<uint32, FVoxelOverlayRegionMessage> Parts;
		TArray<uint8> Assembly;
		TUniquePtr<FVoxelByteReader> Reader;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparationLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
		TMap<FIntVector, TSharedPtr<FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe>> PreparedRegions;
		TMap<FIntVector, bool> SeenSections;
		TArray<FIntVector> ChangedSections;
		FRootPtr PreparedRoot;
		uint32 EntryCount = 0;
		uint32 EntryCursor = 0;
		int32 HeadCursor = 0;
		uint8 Phase = 0;
		const FVoxelOverlayRegionMessage* Header() const { return Cohort ? Cohort.Get() : Parts.Find(0); }
		bool Assemble(TArray<uint8>& Out, FString& Error) const
		{
			if (Parts.Num() != int32(Count)) return false;
			Out.Reserve(TotalBytes);
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const auto* Part = Parts.Find(Index);
				if (!Part) return false;
				Out.Append(Part->Payload);
			}
			FVoxelOverlayHash Actual;
			if (Out.Num() != int32(TotalBytes) || !FVoxelOverlayHash::Compute(Out, Actual) || Actual != Hash)
			{
				Error = TEXT("完整区域正文的长度或 SHA256 不匹配");
				return false;
			}
			return true;
		}
	};
	struct FReceivingRegion
	{
		uint64 StoredBytes = 0;
		FVoxelOverlayRegionMessage Begin;
		FVoxelOverlayRegionMessage End;
		TMap<uint32, FVoxelOverlayRegionMessage> Pages;
		TMap<FIntVector, FPartSet> Payloads;
		TSet<FIntVector> ExpectedPayloadSections;
		TSharedPtr<FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe> Prepared;
		TSharedPtr<const FVoxelOverlayReplicaWork, ESPMode::ThreadSafe> PreparedProof;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparationLease;
		TArray<FVoxelOverlayDirectoryEntry> PreparedEntries;
		TArray<uint8> Assembly;
		int32 EntryCursor = 0;
		bool bDirectoryVerified = false;
		bool bPreparationComplete = false;
		TSharedPtr<const FVoxelOverlayRegionMessage, ESPMode::ThreadSafe> Cohort;
		bool bEnded = false;
		const TArray<FVoxelOverlayRegionHead>& Heads() const { return Cohort ? Cohort->Heads : Begin.Heads; }
	};
}

struct FVoxelOverlayReplicaReceiving
{
	struct FTicket
	{
		FGuid Subscription;
		TArray<FIntVector> Regions;
		TSharedPtr<const FVoxelOverlayRegionMessage, ESPMode::ThreadSafe> Cohort;
		TSharedPtr<const FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe> Candidate;
	};
	TMap<FGuid, FTicket> Tickets;
	TMap<FIntVector, uint64> RequiredSequences;
	TMap<FIntVector, FReceivingRegion> Regions;
	TMap<FGuid, FPartSet> Transactions;
	TMap<FIntVector, EVoxelOverlayReplicaState> States;
	TMap<FGuid, FVoxelOverlayHash> SettledTransactions;
	TArray<FGuid> SettledOrder;
	TArray<FGuid> DeltaOrder;
	TArray<FGuid> TicketOrder;
	TMap<FGuid, uint64> CapacityBlockedTickets;
	int32 DeltaCursor = 0;
	int32 TicketCursor = 0;
	uint64 Bytes = 0;
	uint64 PreparingBytes = 0;
};

struct FVoxelOverlayReplicaLifetime
{
	FVoxelOverlayReplica* Owner = nullptr;
};

struct FReplicaMessageKey
{
	EVoxelMessage Kind = EVoxelMessage::None;
	FIntVector Region;
	FIntVector Section;
	uint32 Index = 0;
	FGuid Snapshot;
	FGuid Transaction;
	bool operator==(const FReplicaMessageKey& InOther) const = default;
};

uint32 GetTypeHash(const FReplicaMessageKey& Key)
{
	uint32 Hash = HashCombineFast(::GetTypeHash(uint8(Key.Kind)), ::GetTypeHash(Key.Region));
	Hash = HashCombineFast(Hash, ::GetTypeHash(Key.Section));
	Hash = HashCombineFast(Hash, ::GetTypeHash(Key.Index));
	Hash = HashCombineFast(Hash, ::GetTypeHash(Key.Snapshot));
	return HashCombineFast(Hash, ::GetTypeHash(Key.Transaction));
}

struct FVoxelOverlayReplicaWork final : FVoxelTaskCustomPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<FVoxelOverlayReplica, ESPMode::ThreadSafe> Worker;
	TSharedPtr<const FVoxelOverlayReplicaRoot, ESPMode::ThreadSafe> Basis;
	TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> Notification;
	TSharedPtr<const FVoxelOverlayRegionMessage, ESPMode::ThreadSafe> Cohort;
	TMap<FIntVector, FGuid> Snapshots;
	TMap<FReplicaMessageKey, FVoxelOverlayHash> MessageDigests;
	FGuid Identity;
	FGuid CoverageTicket;
	FGuid Subscription;
	FVoxelOverlayHash DeltaHash;
	FVoxelTaskStamp Stamp;
	uint64 Bytes = 0;
	uint64 CapacityVersion = 0;
	bool bDelta = false;
	bool bRegionOnly = false;
	bool bCandidate = false;
	TAtomic<bool> bWaitingCapacity{false};

	static FReplicaMessageKey MessageKey(const EVoxelMessage Kind, const FVoxelOverlayRegionMessage& Message)
	{
		return {Kind, Message.Region, Message.Section, Message.Index, Message.SnapshotId, Message.AuthorityTransaction};
	}

	FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load()) { Result.bCanceled = true; return Result; }
		if (bWaitingCapacity.Load() && Worker->Resources->Snapshot().ReleaseVersion <= CapacityVersion)
		{
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}
		const bool bSucceeded = bRegionOnly ? Worker->PrepareReceivingRegion(Stamp.Section, Result.Error) :
			(bDelta ? Worker->TryApplyDelta(Identity, Result.Error) : Worker->TryCommitTicket(Identity, Result.Error));
		if (!bSucceeded) return Result;
		const bool bPrepared = bRegionOnly && Worker->Receiving->Regions.FindChecked(Stamp.Section).bPreparationComplete;
		if (!Notification && !bPrepared)
		{
			// 正常逐步推进不等待资源事件；只有明确的容量拒绝才暂停调度。
			const bool bBlocked = Result.Error.StartsWith(TEXT("CapacityBlocked:"));
			if (bBlocked) CapacityVersion = Worker->Resources->Snapshot().ReleaseVersion;
			bWaitingCapacity.Store(bBlocked);
			Result.Error.Reset();
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}
		bWaitingCapacity.Store(false);
		Result.bSuccess = true;
		return Result;
	}

	virtual uint64 GetAllocatedBytes() const override
	{
		return sizeof(*this) + Snapshots.GetAllocatedSize() + MessageDigests.GetAllocatedSize();
	}
	virtual uint64 GetResourceAccountedBytes() const override
	{
		return InputLease ? InputLease->Bytes() : 0;
	}
};

FVoxelOverlayReplica::FVoxelOverlayReplica(const uint64 InLocalEpoch,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
	: LocalEpoch(InLocalEpoch), Resources(MoveTemp(InResources)), Receiving(MakeUnique<FVoxelOverlayReplicaReceiving>()),
	Lifetime(MakeShared<FVoxelOverlayReplicaLifetime, ESPMode::ThreadSafe>())
{
	Lifetime->Owner = this;
}

FVoxelOverlayReplica::~FVoxelOverlayReplica()
{
	Lifetime->Owner = nullptr;
}

void FVoxelOverlayReplica::SetTaskQueue(TFunction<bool(FVoxelTaskRequest&&)> InQueue)
{
	TaskQueue = MoveTemp(InQueue);
}

void FVoxelOverlayReplica::SetTaskBindingDomain(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	check(InConsumer.IsValid() && InConsumer.WorldEpoch == LocalEpoch && !PendingTask.IsSet());
	TaskBindingResources = InResources;
	TaskBindingConsumer = InConsumer;
}

void FVoxelOverlayReplica::SetTaskCanceler(TFunction<void(const FVoxelTaskStamp&)> InCancel)
{
	TaskCanceler = MoveTemp(InCancel);
}

void FVoxelOverlayReplica::SetAuthority(const uint64 InAuthorityEpoch, const FVoxelOverlayHash& InManifest,
	const FGuid& InDirectoryEpoch)
{
	check(IsInGameThread());
	if (AuthorityEpoch != InAuthorityEpoch || ManifestIdentity != InManifest || DirectoryEpoch != InDirectoryEpoch) Reset();
	AuthorityEpoch = InAuthorityEpoch; ManifestIdentity = InManifest; DirectoryEpoch = InDirectoryEpoch;
}

const FVoxelOverlayRegionSnapshot* FVoxelOverlayReplica::FindRegion(const FIntVector& InRegion) const
{
	const auto* Node = Root.Get();
	for (int32 Bit = 31; Bit >= 0 && Node; --Bit) Node = Node->Children[Branch(InRegion, Bit)].Get();
	return Node ? Node->Region.Get() : nullptr;
}

EVoxelOverlayReplicaState FVoxelOverlayReplica::GetState(const FIntVector& InRegion) const
{
	const auto* State = Receiving->States.Find(InRegion);
	return State ? *State : EVoxelOverlayReplicaState::Unknown;
}

void FVoxelOverlayReplica::MarkGap(const FIntVector& InRegion)
{
	Receiving->States.Add(InRegion, EVoxelOverlayReplicaState::Gap);
}

bool FVoxelOverlayReplica::GetTicketHead(const FGuid& InTicket, const FIntVector& InRegion,
	FVoxelOverlayRegionHead& OutHead) const
{
	const auto* Ticket = Receiving->Tickets.Find(InTicket);
	if (!Ticket || !Ticket->Regions.Contains(InRegion)) return false;
	const auto Region = RegionAt(Ticket->Candidate ? Ticket->Candidate : Root, InRegion);
	if (!Region) return false;
	OutHead.Region = InRegion;
	OutHead.Sequence = Region->Sequence;
	OutHead.Hash = Region->DirectoryHash;
	return true;
}

bool FVoxelOverlayReplica::FailRegion(const FIntVector& InRegion, FString InReason, FString& OutError)
{
	Receiving->States.Add(InRegion, EVoxelOverlayReplicaState::ResyncRequired);
	OutError = MoveTemp(InReason);
	return false;
}

bool FVoxelOverlayReplica::Receive(const EVoxelMessage InKind, FVoxelOverlayRegionMessage&& M, FString& OutError)
{
	check(IsInGameThread());
	if (M.WorldEpoch != AuthorityEpoch || M.ManifestIdentity != ManifestIdentity || M.DirectoryEpoch != DirectoryEpoch) return true;
	auto* Ticket = Receiving->Tickets.Find(M.CoverageTicket);
	if (!Ticket || Ticket->Subscription != M.SubscriptionId || !Ticket->Regions.Contains(M.Region)) return true;
	if (InKind == EVoxelMessage::SnapshotBegin && M.Index == MAX_uint32)
	{
		if (!M.ResourceLease || !M.SnapshotId.IsValid() || M.Heads.Num() != Ticket->Regions.Num() || M.Count != uint32(M.Heads.Num()))
			return FailRegion(M.Region, TEXT("Ticket Head 向量声明不完整"), OutError);
		for (const auto& Head : M.Heads)
			if (!Ticket->Regions.Contains(Head.Region)) return FailRegion(M.Region, TEXT("Ticket Head 超出订阅范围"), OutError);
		if (Ticket->Cohort && Ticket->Cohort->SnapshotId == M.SnapshotId)
			return Ticket->Cohort->WireDigest == M.WireDigest || FailRegion(M.Region, TEXT("重复 Ticket Head 摘要冲突"), OutError);
		const uint64 PreviousBytes = Ticket->Cohort ? Ticket->Cohort->AllocatedBytes() : 0;
		const bool bOldPreparing = Preparing && Preparing->Cohort == Ticket->Cohort && Ticket->Cohort;
		if (Receiving->Bytes - PreviousBytes + Receiving->PreparingBytes + M.AllocatedBytes() +
			(bOldPreparing ? PreviousBytes : 0) > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("Ticket Head 超过连接配额"), OutError);
		if (bOldPreparing) Receiving->PreparingBytes += PreviousBytes;
		if (Preparing && Preparing->CoverageTicket == M.CoverageTicket && TaskCanceler) TaskCanceler(Preparing->Stamp);
		if (PendingTask.IsSet() && Preparing && Preparing->CoverageTicket == M.CoverageTicket) CancelPendingPreparation();
		Receiving->Bytes = Receiving->Bytes - PreviousBytes + M.AllocatedBytes();
		Ticket->Cohort = MakeShared<FVoxelOverlayRegionMessage, ESPMode::ThreadSafe>(MoveTemp(M));
		++ProgressSerial;
		return true;
	}
	if (Preparing)
	{
		const auto* Snapshot = Preparing->Snapshots.Find(M.Region);
		if ((Snapshot && *Snapshot == M.SnapshotId) || (Preparing->bDelta && Preparing->Identity == M.AuthorityTransaction))
		{
			const auto* Digest = Preparing->MessageDigests.Find(FVoxelOverlayReplicaWork::MessageKey(InKind, M));
			return (Digest && *Digest == M.WireDigest) || FailRegion(M.Region, TEXT("准备中事务的重复包内容冲突"), OutError);
		}
	}
	if (const auto* Prepared = Receiving->Regions.Find(M.Region); Prepared && Prepared->bPreparationComplete &&
		Prepared->Begin.SnapshotId == M.SnapshotId && Prepared->PreparedProof)
	{
		const auto* Digest = Prepared->PreparedProof->MessageDigests.Find(FVoxelOverlayReplicaWork::MessageKey(InKind, M));
		return (Digest && *Digest == M.WireDigest) || FailRegion(M.Region, TEXT("已准备区域的重复包内容冲突"), OutError);
	}
	if (!M.ResourceLease)
		return FailRegion(M.Region, TEXT("区域接收超过连接配额，必须重同步"), OutError);
	if (InKind == EVoxelMessage::SnapshotBegin)
	{
		const auto& Heads = M.Heads.IsEmpty() && Ticket->Cohort ? Ticket->Cohort->Heads : M.Heads;
		if (!M.SnapshotId.IsValid() || M.Count > 16 || M.EntryCount > 512 || Heads.Num() != Ticket->Regions.Num() ||
			(M.Heads.IsEmpty() && (Ticket->Cohort->SnapshotId != M.SnapshotId || Ticket->Cohort->AuthorityCut != M.AuthorityCut)) ||
			M.TargetSequence < M.BaseSequence || !Heads.ContainsByPredicate([&M](const auto& Head)
			{ return Head.Region == M.Region && Head.Sequence == M.BaseSequence; }))
			return FailRegion(M.Region, TEXT("目录 Begin 的范围或水位无效"), OutError);
		for (const auto& Head : Heads)
			if (!Ticket->Regions.Contains(Head.Region)) return FailRegion(M.Region, TEXT("Begin 超出 CoverageTicket 的完整范围"), OutError);
		if (const auto* Old = Receiving->Regions.Find(M.Region); Old && Old->Begin.SnapshotId == M.SnapshotId)
			return (Old->Begin.ContentHash == M.ContentHash && Old->Begin.DirectoryHash == M.DirectoryHash && Old->Heads() == Heads) ||
				FailRegion(M.Region, TEXT("重复 Begin 的切点冲突"), OutError);
		FReceivingRegion Region;
		if (M.Heads.IsEmpty()) Region.Cohort = Ticket->Cohort;
		const uint64 PreviousBytes = Receiving->Regions.Contains(M.Region) ? Receiving->Regions.FindChecked(M.Region).StoredBytes : 0;
		if (Receiving->Bytes - PreviousBytes + Receiving->PreparingBytes + M.AllocatedBytes() > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("区域接收超过连接配额，必须重同步"), OutError);
		if (const auto* Old = Receiving->Regions.Find(M.Region)) Receiving->Bytes -= Old->StoredBytes;
		Region.StoredBytes = M.AllocatedBytes();
		Receiving->Bytes += Region.StoredBytes;
		Receiving->RequiredSequences.FindOrAdd(M.Region) = FMath::Max(Receiving->RequiredSequences.FindRef(M.Region), M.TargetSequence);
		Region.Begin = MoveTemp(M);
		Receiving->States.Add(Region.Begin.Region, EVoxelOverlayReplicaState::SnapshotReceiving);
		Receiving->Regions.Add(Region.Begin.Region, MoveTemp(Region));
		++ProgressSerial;
		return true;
	}
	if (InKind == EVoxelMessage::RegionDelta)
	{
		const bool bHeader = M.Index == MAX_uint32;
		if (!M.AuthorityTransaction.IsValid() || M.Count == 0 || M.Count > 512 ||
			(bHeader ? M.Heads.IsEmpty() || !M.Payload.IsEmpty() : M.Index >= M.Count))
			return FailRegion(M.Region, TEXT("区域增量事务身份或分片无效"), OutError);
		if (const auto* Settled = Receiving->SettledTransactions.Find(M.AuthorityTransaction))
			return *Settled == M.ContentHash || FailRegion(M.Region, TEXT("重复增量事务的 Hash 冲突"), OutError);
		auto& Parts = Receiving->Transactions.FindOrAdd(M.AuthorityTransaction);
		if (!Parts.Count)
		{
			Parts.Count = M.Count; Parts.TotalBytes = M.TotalBytes; Parts.Hash = M.ContentHash;
			Receiving->DeltaOrder.Add(M.AuthorityTransaction);
		}
		if (Parts.Count != M.Count || Parts.TotalBytes != M.TotalBytes || Parts.Hash != M.ContentHash)
			return FailRegion(M.Region, TEXT("增量分片跨代混合"), OutError);
		const auto* ExistingHeader = Parts.Header();
		if (!ExistingHeader && !Parts.Parts.IsEmpty()) ExistingHeader = &Parts.Parts.CreateConstIterator().Value();
		if (ExistingHeader && (ExistingHeader->AuthorityCut != M.AuthorityCut || ExistingHeader->DirectoryRevision != M.DirectoryRevision ||
			ExistingHeader->SubscriptionId != M.SubscriptionId || ExistingHeader->CoverageTicket != M.CoverageTicket ||
			(!ExistingHeader->Heads.IsEmpty() && !M.Heads.IsEmpty() && ExistingHeader->Heads != M.Heads)))
			return FailRegion(M.Region, TEXT("增量分片的完整事务元组不一致"), OutError);
		if (bHeader && Parts.Cohort)
			return Parts.Cohort->WireDigest == M.WireDigest || FailRegion(M.Region, TEXT("重复增量 Head 摘要冲突"), OutError);
		if (!bHeader)
			if (const auto* Previous = Parts.Parts.Find(M.Index))
				return Previous->WireDigest == M.WireDigest || FailRegion(M.Region, TEXT("重复增量分片内容冲突"), OutError);
		if (Receiving->Bytes + Receiving->PreparingBytes + M.AllocatedBytes() > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("区域增量超过连接配额，必须重同步"), OutError);
		for (const auto& Head : M.Heads)
		{
			if (!Ticket->Regions.Contains(Head.Region) || Head.PreviousSequence == MAX_uint64 || Head.Sequence != Head.PreviousSequence + 1)
				return FailRegion(M.Region, TEXT("增量 Head 越出订阅或不连续"), OutError);
			Receiving->RequiredSequences.FindOrAdd(Head.Region) = FMath::Max(Receiving->RequiredSequences.FindRef(Head.Region), Head.Sequence);
			const auto Current = RegionAt(Ticket->Candidate ? Ticket->Candidate : Root, Head.Region);
			if (!Current || Head.PreviousSequence > Current->Sequence) MarkGap(Head.Region);
		}
		const FGuid Transaction = M.AuthorityTransaction;
		Parts.StoredBytes += M.AllocatedBytes();
		Receiving->Bytes += M.AllocatedBytes();
		if (bHeader) Parts.Cohort = MakeShared<FVoxelOverlayRegionMessage, ESPMode::ThreadSafe>(MoveTemp(M));
		else Parts.Parts.Add(M.Index, MoveTemp(M));
		++ProgressSerial;
		return TryApplyDelta(Transaction, OutError);
	}
	auto* Region = Receiving->Regions.Find(M.Region);
	if (!Region || M.SnapshotId != Region->Begin.SnapshotId || M.SubscriptionId != Region->Begin.SubscriptionId ||
		M.CoverageTicket != Region->Begin.CoverageTicket) return true;
	if (InKind == EVoxelMessage::DirectoryPage)
	{
		if (M.Index >= Region->Begin.Count || M.Count != Region->Begin.Count)
			return FailRegion(M.Region, TEXT("目录页号或页总数不符"), OutError);
		FVoxelOverlayHash Hash;
		if (!FVoxelOverlayRegionCodec::DirectoryHash(M.Entries, Hash) || Hash != M.ContentHash)
			return FailRegion(M.Region, TEXT("目录页的规范条目 Hash 不符"), OutError);
		for (const auto& Entry : M.Entries)
			if (FVoxelOverlayRegionCodec::RegionOf(Entry.Section) != M.Region)
				return FailRegion(M.Region, TEXT("目录条目越出声明 Region"), OutError);
		if (const auto* Previous = Region->Pages.Find(M.Index))
			return Previous->Entries == M.Entries || FailRegion(M.Region, TEXT("重复目录页内容冲突"), OutError);
		if (Receiving->Bytes + Receiving->PreparingBytes + M.AllocatedBytes() > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("目录页超过连接配额，必须重同步"), OutError);
		Region->StoredBytes += M.AllocatedBytes();
		Receiving->Bytes += M.AllocatedBytes();
		for (const auto& Entry : M.Entries) if (!Entry.bTombstone) Region->ExpectedPayloadSections.Add(Entry.Section);
		Region->Pages.Add(M.Index, MoveTemp(M));
		++ProgressSerial;
	}
	else if (InKind == EVoxelMessage::OverlayPayload)
	{
		if (M.Count == 0 || M.Count > 64 || M.Index >= M.Count || M.TotalBytes > FVoxelNetworkCodec::MaxRepresentationBytes ||
			FVoxelOverlayRegionCodec::RegionOf(M.Section) != M.Region)
			return FailRegion(M.Region, TEXT("Overlay 分片范围不合法"), OutError);
		auto& Parts = Region->Payloads.FindOrAdd(M.Section);
		if (!Parts.Count) { Parts.Count = M.Count; Parts.TotalBytes = M.TotalBytes; Parts.Hash = M.ContentHash; }
		if (Parts.Count != M.Count || Parts.TotalBytes != M.TotalBytes || Parts.Hash != M.ContentHash)
			return FailRegion(M.Region, TEXT("Overlay 分片跨代混合"), OutError);
		if (const auto* Previous = Parts.Parts.Find(M.Index))
			return Previous->Payload == M.Payload || FailRegion(M.Region, TEXT("重复 Overlay 分片内容冲突"), OutError);
		if (Receiving->Bytes + Receiving->PreparingBytes + M.AllocatedBytes() > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("Overlay 正文超过连接配额，必须重同步"), OutError);
		Parts.StoredBytes += M.AllocatedBytes();
		Region->StoredBytes += M.AllocatedBytes();
		Receiving->Bytes += M.AllocatedBytes();
		Parts.Parts.Add(M.Index, MoveTemp(M));
		++ProgressSerial;
	}
	else if (InKind == EVoxelMessage::SnapshotEnd)
	{
		if (M.BaseSequence != Region->Begin.BaseSequence || M.TargetSequence < M.BaseSequence || M.AuthorityCut != Region->Begin.AuthorityCut)
			return FailRegion(M.Region, TEXT("SnapshotEnd 与 Begin 切点不符"), OutError);
		if (Region->bEnded)
			return Region->End.WireDigest == M.WireDigest || FailRegion(M.Region, TEXT("重复 SnapshotEnd 的内容冲突"), OutError);
		if (Receiving->Bytes + Receiving->PreparingBytes + M.AllocatedBytes() > MaximumReceivingBytes)
			return FailRegion(M.Region, TEXT("SnapshotEnd 超过连接配额，必须重同步"), OutError);
		Region->Begin.TargetSequence = M.TargetSequence;
		Receiving->RequiredSequences.FindOrAdd(M.Region) = FMath::Max(Receiving->RequiredSequences.FindRef(M.Region), M.TargetSequence);
		Region->bEnded = true;
		Receiving->TicketOrder.AddUnique(M.CoverageTicket);
		Region->StoredBytes += M.AllocatedBytes();
		Receiving->Bytes += M.AllocatedBytes();
		Region->End = MoveTemp(M);
		++ProgressSerial;
	}
	else return true;
	return TryCommitTicket(Region->Begin.CoverageTicket, OutError);
}

bool FVoxelOverlayReplica::QueuePreparation(const FGuid& InIdentity, const bool bInDelta, FString& OutError)
{
	check(IsInGameThread());
	if (Preparing) return PendingTask.IsSet() ? RetryPendingPreparation(OutError) : true;
	const FVoxelOverlayRegionMessage* Header = nullptr;
	TArray<FIntVector, TInlineAllocator<FVoxelOverlayRegionCodec::MaximumRegions>> Keys;
	bool bRegionOnly = false;
	bool bCandidate = false;
	FRootPtr Basis = Root;
	if (bInDelta)
	{
		const auto* Parts = Receiving->Transactions.Find(InIdentity);
		if (!Parts || Parts->Parts.Num() != int32(Parts->Count)) return true;
		Header = Parts->Header();
		if (!Header) return true;
		if (Header->Heads.IsEmpty()) return true;
		const auto* Ticket = Receiving->Tickets.Find(Header->CoverageTicket);
		if (!Ticket) return true;
		bCandidate = Ticket->Candidate.IsValid();
		if (bCandidate) Basis = Ticket->Candidate;
		for (const auto& Head : Header->Heads)
		{
			const auto Region = RegionAt(Basis, Head.Region);
			if (!Region || Region->Sequence < Head.PreviousSequence) { MarkGap(Head.Region); return true; }
			if (Region->Sequence != Head.PreviousSequence || Region->DirectoryHash != Head.BeforeHash)
				return FailRegion(Head.Region, TEXT("增量前置区域版本冲突"), OutError);
			Keys.Add(Head.Region);
		}
	}
	else
	{
		const auto* Ticket = Receiving->Tickets.Find(InIdentity);
		if (!Ticket) return true;
		bool bAllPrepared = true;
		for (const auto& Key : Ticket->Regions)
		{
			const auto* Region = Receiving->Regions.Find(Key);
			if (!Region || Region->Begin.CoverageTicket != InIdentity) { bAllPrepared = false; continue; }
			if (Region->bPreparationComplete) continue;
			bAllPrepared = false;
			if (!Region->bEnded || Region->Pages.Num() != int32(Region->Begin.Count)) continue;
			if (Region->Payloads.Num() > Region->ExpectedPayloadSections.Num())
				return FailRegion(Key, TEXT("目录外的 Overlay 正文不能进入快照"), OutError);
			bool bReady = true;
			for (const auto& Section : Region->ExpectedPayloadSections)
			{
				const auto* Payload = Region->Payloads.Find(Section);
				if (!Payload || Payload->Parts.Num() != int32(Payload->Count)) { bReady = false; break; }
			}
			if (!bReady) continue;
			Header = &Region->Begin;
			Keys.Add(Key);
			bRegionOnly = true;
			break;
		}
		if (!bRegionOnly)
		{
			if (!bAllPrepared) return true;
			if (Ticket->Candidate)
			{
				// 候选目录达到整组声明水位后才与最新可采样根合并。
				for (const auto& Key : Ticket->Regions)
				{
					const auto Region = RegionAt(Ticket->Candidate, Key);
					if (!Region || Region->Sequence < Receiving->RequiredSequences.FindRef(Key)) return true;
				}
			}
			for (const auto& Key : Ticket->Regions)
			{
				const auto& Region = Receiving->Regions.FindChecked(Key);
				if (!Header) Header = &Region.Begin;
				if (Region.Begin.AuthorityCut != Header->AuthorityCut ||
					(Region.Cohort != Ticket->Cohort && Region.Heads() != Receiving->Regions.FindChecked(Ticket->Regions[0]).Heads()))
					return FailRegion(Key, TEXT("整组目录不属于同一 Ticket 切点"), OutError);
				Keys.Add(Key);
			}
		}
	}
	if (!Header || Keys.IsEmpty()) return true;
	if (!TaskQueue) { OutError = TEXT("Replica 验证 Worker 尚未接入 Scheduler"); return false; }
	if (!TaskBindingResources || !TaskBindingConsumer.IsValid() || TaskBindingConsumer.WorldEpoch != LocalEpoch)
	{
		OutError = TEXT("InvalidConsumer: Replica task binding domain is not initialized");
		return false;
	}
	int32 MessageCount = 0;
	uint64 CopyBytes = 0;
	if (bInDelta)
	{
		const auto& Parts = Receiving->Transactions.FindChecked(InIdentity);
		MessageCount = Parts.Parts.Num() + (Parts.Cohort ? 1 : 0);
	}
	else for (const auto& Key : Keys)
	{
		const auto& Region = Receiving->Regions.FindChecked(Key);
		if (bRegionOnly)
		{
			MessageCount += 2 + Region.Pages.Num();
			for (const auto& Payload : Region.Payloads) MessageCount += Payload.Value.Parts.Num();
		}
		else CopyBytes += Region.Begin.Heads.GetAllocatedSize() + Region.End.Heads.GetAllocatedSize();
	}
	const int32 RegionCount = Keys.Num();
	const uint64 Metadata = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayReplicaWork>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelOverlayReplica>() + sizeof(FVoxelOverlayReplicaReceiving) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FReceivingRegion>(bInDelta ? 0 : RegionCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(RegionCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, EVoxelOverlayReplicaState>(RegionCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FGuid>(RegionCount) +
		VoxelBoundedAllocation::MapBytes<FReplicaMessageKey, FVoxelOverlayHash>(MessageCount) +
		VoxelBoundedAllocation::MapBytes<FGuid, FPartSet>(bInDelta ? 1 : 0) +
		VoxelBoundedAllocation::MapBytes<FGuid, FVoxelOverlayHash>(1) +
		VoxelBoundedAllocation::ArrayBytes<FGuid>(1) * 3 + CopyBytes + 512;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, OwnerIncarnation,
		Header->Region, ++NextSerial), EVoxelTerrainResourcePool::Input, Metadata, Lease)) return true;
	auto Work = MakeShared<FVoxelOverlayReplicaWork, ESPMode::ThreadSafe>();
	Work->InputLease = MoveTemp(Lease);
	Work->Identity = InIdentity;
	Work->CoverageTicket = Header->CoverageTicket;
	Work->Subscription = Header->SubscriptionId;
	Work->bDelta = bInDelta;
	Work->bRegionOnly = bRegionOnly;
	Work->bCandidate = bCandidate;
	Work->Basis = Basis;
	Work->Snapshots.Reserve(RegionCount);
	Work->MessageDigests.Reserve(MessageCount);
	Work->Worker = MakeShared<FVoxelOverlayReplica, ESPMode::ThreadSafe>(LocalEpoch, Resources);
	Work->Worker->bDetachedWorker = true;
	Work->Worker->AuthorityEpoch = AuthorityEpoch;
	Work->Worker->ManifestIdentity = ManifestIdentity;
	Work->Worker->DirectoryEpoch = DirectoryEpoch;
	Work->Worker->Root = Basis;
	Work->Worker->Receiving->Regions.Reserve(bInDelta ? 0 : RegionCount);
	Work->Worker->Receiving->RequiredSequences.Reserve(RegionCount);
	Work->Worker->Receiving->States.Reserve(RegionCount);
	const auto* Expected = Receiving->Tickets.Find(Work->CoverageTicket);
	if (Expected) Work->Cohort = Expected->Cohort;
	const auto Remember = [&Work](const EVoxelMessage Kind, const FVoxelOverlayRegionMessage& Message)
	{
		Work->MessageDigests.Add(FVoxelOverlayReplicaWork::MessageKey(Kind, Message), Message.WireDigest);
	};
	if (bInDelta)
	{
		auto& Parts = Receiving->Transactions.FindChecked(InIdentity);
		Work->Bytes = Parts.StoredBytes;
		Work->DeltaHash = Parts.Hash;
		if (Parts.Cohort) Remember(EVoxelMessage::RegionDelta, *Parts.Cohort);
		for (const auto& Pair : Parts.Parts) Remember(EVoxelMessage::RegionDelta, Pair.Value);
		Work->Worker->Receiving->Transactions.Add(InIdentity, MoveTemp(Parts));
		Receiving->Transactions.Remove(InIdentity);
		Receiving->DeltaOrder.Remove(InIdentity);
	}
	else for (const auto& Key : Keys)
	{
		auto& Region = Receiving->Regions.FindChecked(Key);
		Work->Snapshots.Add(Key, Region.Begin.SnapshotId);
		Work->Worker->Receiving->RequiredSequences.Add(Key, Receiving->RequiredSequences.FindRef(Key));
		if (bRegionOnly)
		{
			Work->Bytes = Region.StoredBytes;
			Remember(EVoxelMessage::SnapshotBegin, Region.Begin);
			Remember(EVoxelMessage::SnapshotEnd, Region.End);
			for (const auto& Page : Region.Pages) Remember(EVoxelMessage::DirectoryPage, Page.Value);
			for (const auto& Payload : Region.Payloads)
				for (const auto& Part : Payload.Value.Parts) Remember(EVoxelMessage::OverlayPayload, Part.Value);
			Work->Worker->Receiving->Regions.Add(Key, MoveTemp(Region));
			Receiving->Regions.Remove(Key);
		}
		else
		{
			FReceivingRegion Frozen;
			Frozen.Begin = Region.Begin;
			Frozen.End = Region.End;
			Frozen.Cohort = Region.Cohort;
			Frozen.Prepared = Expected && Expected->Candidate ?
				ConstCastSharedPtr<FVoxelOverlayRegionSnapshot>(RegionAt(Expected->Candidate, Key)) : Region.Prepared;
			Frozen.bEnded = Frozen.bPreparationComplete = true;
			Work->Worker->Receiving->Regions.Add(Key, MoveTemp(Frozen));
		}
	}
	Receiving->Bytes -= Work->Bytes;
	Receiving->PreparingBytes = Work->Bytes;
	Work->Worker->Receiving->Bytes = Work->Bytes;
	const TWeakPtr<FVoxelOverlayReplicaWork, ESPMode::ThreadSafe> WeakWork = Work;
	Work->Worker->OnCommitted = [WeakWork](TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> Notification)
	{
		if (const auto Active = WeakWork.Pin()) Active->Notification = MoveTemp(Notification);
	};
	PendingTask.Emplace();
	auto& Task = PendingTask.GetValue();
	Task.Kind = EVoxelTaskKind::NetworkRepresentation;
	Task.Stamp.OwnerId = OwnerId;
	Task.Stamp.OwnerIncarnation = OwnerIncarnation;
	Task.Stamp.WorldEpoch = LocalEpoch;
	Task.Stamp.Section = Keys[0];
	Task.Stamp.Token = ++NextSerial;
	Work->Stamp = Task.Stamp;
	Task.WorkClass = EVoxelWorkClass::Visible;
	Task.ServiceBand = EVoxelTerrainServiceBand::P2;
	Task.SourceStableKey = OwnerId;
	Task.FirstRunnableSeconds = FPlatformTime::Seconds();
	Task.bResumable = true;
	Task.ReservedBytes = 1;
	Preparing = Work;
	return RetryPendingPreparation(OutError);
}

bool FVoxelOverlayReplica::RetryPendingPreparation(FString& OutError)
{
	if (!PendingTask.IsSet() || !Preparing) return true;
	if (bBindingCapacityBlocked)
	{
		uint64 Current = 0;
		if (!TaskBindingResources->TryGetReleaseVersionExcludingOwner(TaskBindingConsumer, Current) || Current <= BindingCapacityVersion)
		{
			OutError.Reset();
			return true;
		}
		bBindingCapacityBlocked = false;
	}
	const auto Work = Preparing;
	auto& Task = PendingTask.GetValue();
	const uint64 BeforeBinding = TaskBindingResources->ReleaseVersionExcludingOwner(TaskBindingConsumer);
	auto GetBlockReason = [Work, WorkResources = Resources]
	{
		return Work->bWaitingCapacity.Load() && WorkResources->Snapshot().ReleaseVersion <= Work->CapacityVersion ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	auto Execute = [Work](const TAtomic<bool>& Cancel)
	{
		auto Result = Work->Advance(Cancel);
		if (Result.bSuccess && !Result.bContinue) Result.CustomPayload = Work;
		return Result;
	};
	const TWeakPtr<FVoxelOverlayReplicaLifetime, ESPMode::ThreadSafe> WeakLifetime = Lifetime;
	auto Apply = [WeakLifetime, Work](FVoxelTaskResult&& Result)
	{
		if (const auto Owner = WeakLifetime.Pin(); Owner && Owner->Owner) Owner->Owner->ApplyPreparation(Work, MoveTemp(Result));
	};
	if ((!Task.bBlockBound && !Task.BindGetBlockReason(TaskBindingResources.ToSharedRef(), TaskBindingConsumer, MoveTemp(GetBlockReason), OutError)) ||
		(!Task.bExecuteBound && !Task.BindExecute(TaskBindingResources.ToSharedRef(), TaskBindingConsumer, MoveTemp(Execute), OutError)) ||
		(!Task.bApplyBound && !Task.BindApply(TaskBindingResources.ToSharedRef(), TaskBindingConsumer, MoveTemp(Apply), OutError)))
	{
		if (!OutError.StartsWith(TEXT("CapacityBlocked"))) return false;
		bBindingCapacityBlocked = true;
		BindingCapacityVersion = BeforeBinding;
		OutError.Reset();
		return true;
	}
	FVoxelTaskRequest Candidate = MoveTemp(Task);
	PendingTask.Reset();
	if (TaskQueue(MoveTemp(Candidate)))
	{
		OutError.Reset();
		return true;
	}
	if (Preparing != Work || PendingTask.IsSet())
	{
		OutError.Reset();
		return true;
	}
	PendingTask.Emplace(MoveTemp(Candidate));
	if (PendingTask->Blocked == EVoxelTaskBlockReason::Oversized || PendingTask->Blocked == EVoxelTaskBlockReason::TerminalFailure)
	{
		OutError = TEXT("Oversized: Replica original task admission is unreachable or invalid");
		return false;
	}
	bBindingCapacityBlocked = (uint16(PendingTask->Blocked) & (uint16(EVoxelTaskBlockReason::Memory) | uint16(EVoxelTaskBlockReason::InputMemory))) != 0;
	BindingCapacityVersion = BeforeBinding;
	OutError.Reset();
	return true;
}

void FVoxelOverlayReplica::CancelPendingPreparation()
{
	check(PendingTask.IsSet());
	PendingTask.Reset();
	bBindingCapacityBlocked = false;
	Receiving->PreparingBytes = 0;
	Preparing.Reset();
}
void FVoxelOverlayReplica::ApplyPreparation(TSharedPtr<FVoxelOverlayReplicaWork, ESPMode::ThreadSafe> Work,
	FVoxelTaskResult&& Result)
{
	check(IsInGameThread());
	if (Preparing != Work || Result.Stamp != Work->Stamp || Work->Stamp.OwnerIncarnation != OwnerIncarnation) return;
	auto* Ticket = Receiving->Tickets.Find(Work->CoverageTicket);
	const bool bTicketAlive = Ticket && Ticket->Subscription == Work->Subscription &&
		(!Work->Cohort || (Ticket->Cohort && Ticket->Cohort->SnapshotId == Work->Cohort->SnapshotId));
	const bool bBasisCurrent = Work->bRegionOnly || (Work->bCandidate ? Ticket && Ticket->Candidate == Work->Basis : Root == Work->Basis);
	bool bConflict = false;
	for (const auto& Pair : Work->Snapshots)
		bConflict |= GetState(Pair.Key) == EVoxelOverlayReplicaState::ResyncRequired;
	if (Work->Notification) for (const auto& Head : Work->Notification->Heads)
		bConflict |= GetState(Head.Region) == EVoxelOverlayReplicaState::ResyncRequired;
	const bool bValid = Result.bSuccess && !Result.bCanceled && bTicketAlive && bBasisCurrent && !bConflict &&
		Work->Worker->AuthorityEpoch == AuthorityEpoch && Work->Worker->DirectoryEpoch == DirectoryEpoch &&
		Work->Worker->ManifestIdentity == ManifestIdentity;
	if (bValid && Work->bRegionOnly)
	{
		auto& Prepared = Work->Worker->Receiving->Regions.FindChecked(Work->Stamp.Section);
		FReceivingRegion Ready;
		Ready.Begin = MoveTemp(Prepared.Begin);
		Ready.End = MoveTemp(Prepared.End);
		Ready.Cohort = Prepared.Cohort;
		Ready.Prepared = Prepared.Prepared;
		Ready.PreparedProof = Work;
		Ready.bEnded = Ready.bDirectoryVerified = Ready.bPreparationComplete = true;
		Ready.StoredBytes = Ready.Begin.AllocatedBytes() + Ready.End.AllocatedBytes();
		Receiving->Bytes += Ready.StoredBytes;
		Receiving->Regions.Add(Work->Stamp.Section, MoveTemp(Ready));
		++ProgressSerial;
	}
	else if (bValid && Work->Notification)
	{
		++ProgressSerial;
		bool bWaterReady = true;
		for (const auto& Key : Ticket->Regions)
		{
			const auto Region = RegionAt(Work->Worker->Root, Key);
			if (!Region || Region->Sequence < Receiving->RequiredSequences.FindRef(Key)) { bWaterReady = false; break; }
		}
		if (Work->bCandidate || (!Work->bDelta && !bWaterReady))
		{
			// 初始追赶期间只推进私有候选根；可采样根和通知仍保持上一个完整切点。
			Ticket->Candidate = Work->Worker->Root;
			for (const auto& Key : Ticket->Regions) Receiving->States.Add(Key, EVoxelOverlayReplicaState::CatchingUp);
			Receiving->TicketOrder.AddUnique(Work->CoverageTicket);
		}
		else
		{
			Root = Work->Worker->Root;
			for (const auto& Head : Work->Notification->Heads)
				Receiving->States.Add(Head.Region, Head.Sequence >= Receiving->RequiredSequences.FindRef(Head.Region) ?
					EVoxelOverlayReplicaState::Complete : EVoxelOverlayReplicaState::Gap);
			if (!Work->bDelta)
			{
				Ticket->Candidate.Reset();
				Receiving->TicketOrder.Remove(Work->CoverageTicket);
				for (const auto& Key : Ticket->Regions)
					if (const auto* Pending = Receiving->Regions.Find(Key); Pending && Pending->Begin.CoverageTicket == Work->CoverageTicket)
					{
						Receiving->Bytes -= Pending->StoredBytes;
						Receiving->Regions.Remove(Key);
					}
			}
			if (OnCommitted) OnCommitted(Work->Notification);
		}
		if (Work->bDelta)
		{
			Receiving->SettledTransactions.Add(Work->Identity, Work->DeltaHash);
			Receiving->SettledOrder.Add(Work->Identity);
			if (Receiving->SettledOrder.Num() > 256)
			{
				Receiving->SettledTransactions.Remove(Receiving->SettledOrder[0]);
				Receiving->SettledOrder.RemoveAt(0, 1, EAllowShrinking::No);
			}
		}
	}
	else if (!Result.bCanceled && bTicketAlive)
	{
		for (const auto& Pair : Work->Snapshots) Receiving->States.Add(Pair.Key, EVoxelOverlayReplicaState::ResyncRequired);
		if (Work->Notification) for (const auto& Head : Work->Notification->Heads)
			Receiving->States.Add(Head.Region, EVoxelOverlayReplicaState::ResyncRequired);
		UE_LOG(LogTemp, Warning, TEXT("Replica 准备不能提交，保留旧根：%s"), *Result.Error);
	}
	Receiving->PreparingBytes = 0;
	Preparing.Reset();
	// 区域证明只保留摘要与输入元数据租约；原始页、正文分片和旧根在后台退休。
	auto RetiringWorker = MoveTemp(Work->Worker);
	auto RetiringBasis = MoveTemp(Work->Basis);
	Work->Notification.Reset();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[RetiringWorker = MoveTemp(RetiringWorker), RetiringBasis = MoveTemp(RetiringBasis)] {});
}
bool FVoxelOverlayReplica::PrepareReceivingRegion(const FIntVector& InRegion, FString& OutError)
{
	auto& Pending = Receiving->Regions.FindChecked(InRegion);
	if (Pending.bPreparationComplete) return true;
	if (!Pending.PreparationLease)
	{
		uint64 MaximumPayload = 0;
		for (const auto& Payload : Pending.Payloads) MaximumPayload = FMath::Max(MaximumPayload, uint64(Payload.Value.TotalBytes));
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(Pending.Begin.EntryCount) +
			VoxelBoundedAllocation::ArrayBytes<uint8>(MaximumPayload) +
			2 * VoxelBoundedAllocation::InitialGrowBytes<uint8>(Pending.Begin.EntryCount * 128 + 4);
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, InRegion, ++NextSerial),
			EVoxelTerrainResourcePool::Working, Bytes, Pending.PreparationLease))
		{
			OutError = TEXT("CapacityBlocked: Region 验证 scratch 尚未准入");
			return true;
		}
		Pending.PreparedEntries.Reserve(Pending.Begin.EntryCount);
		Pending.Assembly.Reserve(MaximumPayload);
	}
	if (!Pending.bDirectoryVerified)
	{
		for (uint32 Page = 0; Page < Pending.Begin.Count; ++Page)
		{
			const auto* Message = Pending.Pages.Find(Page);
			if (!Message) { OutError = TEXT("快照准备缺少完整目录页"); return false; }
			Pending.PreparedEntries.Append(Message->Entries);
		}
		FVoxelOverlayHash DirectoryHash;
		FVoxelOverlayHash SnapshotHash;
		const auto* Head = Pending.Heads().FindByPredicate([&InRegion](const auto& Value) { return Value.Region == InRegion; });
		if (!Head || Pending.PreparedEntries.Num() != int32(Pending.Begin.EntryCount) ||
			!FVoxelOverlayRegionCodec::DirectoryHash(Pending.PreparedEntries, DirectoryHash) ||
			DirectoryHash != Pending.Begin.DirectoryHash || DirectoryHash != Head->Hash ||
			!FVoxelOverlayRegionCodec::SnapshotHash(Pending.PreparedEntries, SnapshotHash) || SnapshotHash != Pending.Begin.ContentHash)
			return FailRegion(InRegion, TEXT("全目录条数、排序或完整性不符"), OutError);
		Pending.bDirectoryVerified = true;
	}
	if (!Pending.Prepared)
	{
		const int32 Count = Pending.PreparedEntries.Num();
		const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayRegionSnapshot>() +
			VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelOverlayDirectoryEntry>(Count) +
			VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(Count);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, InRegion, ++NextSerial),
			EVoxelTerrainResourcePool::Resident, Bytes, Lease))
		{
			OutError = TEXT("CapacityBlocked: Region 不可变目录尚未准入");
			return true;
		}
		auto Snapshot = MakeShared<FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe>();
		Snapshot->ResourceLease = MoveTemp(Lease);
		Snapshot->Region = InRegion;
		Snapshot->DirectoryEpoch = DirectoryEpoch;
		Snapshot->InitialCoverageTicket = Pending.Begin.CoverageTicket;
		Snapshot->DirectoryRevision = Pending.Begin.DirectoryRevision;
		Snapshot->Sequence = Pending.Begin.BaseSequence;
		Snapshot->AuthorityCut = Pending.Begin.AuthorityCut;
		Snapshot->DirectoryHash = Pending.Begin.DirectoryHash;
		Snapshot->Directory.Reserve(Count);
		Snapshot->Overlays.Reserve(Count);
		Pending.Prepared = MoveTemp(Snapshot);
	}
	if (Pending.PreparedEntries.IsValidIndex(Pending.EntryCursor))
	{
		const auto& Entry = Pending.PreparedEntries[Pending.EntryCursor];
		if (!Entry.bTombstone)
		{
			const auto* Parts = Pending.Payloads.Find(Entry.Section);
			if (!Parts || Parts->Hash != Entry.PayloadHash || Parts->TotalBytes != Entry.PayloadBytes)
				return FailRegion(InRegion, TEXT("Overlay 正文不属于目录声明版本"), OutError);
			Pending.Assembly.Reset();
			if (!Parts->Assemble(Pending.Assembly, OutError)) return false;
			uint64 Capacity = 0;
			if (!FVoxelOverlayRegionCodec::OverlayCapacity(Pending.Assembly, Capacity))
				return FailRegion(InRegion, TEXT("Overlay 正文规范容量或编码无效"), OutError);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, Entry.Section, ++NextSerial),
				EVoxelTerrainResourcePool::Resident, Capacity, Lease))
			{
				OutError = TEXT("CapacityBlocked: Overlay 正文驻留配额不足");
				return true;
			}
			auto Overlay = MakeShared<FVoxelOverlaySnapshot, ESPMode::ThreadSafe>();
			if (!FVoxelOverlayRegionCodec::DecodeOverlay(Pending.Assembly, *Overlay, OutError) ||
				Overlay->Section != Entry.Section || Overlay->Revision != Entry.Revision)
				return FailRegion(InRegion, TEXT("Overlay 正文 Section 或版本不符"), OutError);
			Overlay->ResourceLease = MoveTemp(Lease);
			Pending.Prepared->Overlays.Add(Entry.Section, MoveTemp(Overlay));
		}
		Pending.Prepared->Directory.Add(Entry.Section, Entry);
		++Pending.EntryCursor;
		return true;
	}
	Pending.bPreparationComplete = true;
	Pending.PreparedEntries.Empty();
	Pending.Assembly.Empty();
	Pending.PreparationLease.Reset();
	return true;
}

bool FVoxelOverlayReplica::TryCommitTicket(const FGuid& InTicket, FString& OutError)
{
	if (!bDetachedWorker) return QueuePreparation(InTicket, false, OutError);
	FReceivingRegion* First = nullptr;
	for (auto& Pair : Receiving->Regions) if (Pair.Value.Begin.CoverageTicket == InTicket) { First = &Pair.Value; break; }
	if (!First) return true;
	// 首个接收记录在提交后会退休，循环仍需要稳定的完整 Head 向量。
	TArray<FVoxelOverlayRegionHead, TInlineAllocator<FVoxelOverlayRegionCodec::MaximumRegions>> Heads;
	Heads.Append(First->Heads());
	const uint64 Cut = First->Begin.AuthorityCut;
	for (const auto& Head : Heads)
	{
		auto* Pending = Receiving->Regions.Find(Head.Region);
		if (!Pending || Pending->Begin.CoverageTicket != InTicket || !Pending->bEnded ||
			Pending->Begin.AuthorityCut != Cut || Pending->Heads() != Heads) return true;
		if (Pending->bPreparationComplete) continue;
		if (Pending->Pages.Num() != int32(Pending->Begin.Count)) return true;
		if (!PrepareReceivingRegion(Head.Region, OutError)) return false;
		if (!Pending->bPreparationComplete) return true;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, Heads[0].Region, ++NextSerial),
		EVoxelTerrainResourcePool::Resident, 33 * Heads.Num() * RootNodeBytes(), PathLease, 33 * Heads.Num()))
	{
		Receiving->CapacityBlockedTickets.Add(InTicket, Resources->Snapshot().ReleaseVersion);
		OutError = TEXT("CapacityBlocked: Ticket 根路径尚未准入");
		return true;
	}
	FRootPtr NextRoot = Root;
	TArray<FIntVector> Changed;
	for (const auto& Head : Heads)
	{
		auto& Pending = Receiving->Regions.FindChecked(Head.Region);
		if (const auto* Existing = FindRegion(Head.Region); Existing && Existing->Sequence > Pending.Prepared->Sequence)
			return FailRegion(Head.Region, TEXT("初始 Ticket 水位落后于已提交根，必须统一重同步"), OutError);
		NextRoot = SetRegion(NextRoot, Pending.Prepared, 31, PathLease);
		for (const auto& Pair : Pending.Prepared->Directory) Changed.Add(Pair.Key);
		if (const auto* Old = FindRegion(Head.Region)) for (const auto& Pair : Old->Directory) Changed.AddUnique(Pair.Key);
	}
	for (auto& Head : Heads)
	{
		const auto& Prepared = Receiving->Regions.FindChecked(Head.Region).Prepared;
		Head.Sequence = Prepared->Sequence;
		Head.Hash = Prepared->DirectoryHash;
	}
	const auto Notification = PrepareNotification(FGuid(), MoveTemp(Changed), Heads);
	if (!Notification) { OutError = TEXT("CapacityBlocked: 提交通知尚未准入"); return true; }
	Root = MoveTemp(NextRoot);
	Receiving->CapacityBlockedTickets.Remove(InTicket);
	Receiving->TicketOrder.Remove(InTicket);
	for (const auto& Head : Heads)
	{
		const auto& Pending = Receiving->Regions.FindChecked(Head.Region);
		Receiving->States.Add(Head.Region, Pending.Begin.BaseSequence >=
			FMath::Max(Pending.Begin.TargetSequence, Receiving->RequiredSequences.FindRef(Head.Region)) ?
			EVoxelOverlayReplicaState::Complete : EVoxelOverlayReplicaState::CatchingUp);
	}
	if (OnCommitted) OnCommitted(Notification);
	for (const auto& Head : Heads)
	{
		Receiving->Bytes -= Receiving->Regions.FindChecked(Head.Region).StoredBytes;
		Receiving->Regions.Remove(Head.Region);
	}
	TArray<FGuid> Transactions;
	Receiving->Transactions.GetKeys(Transactions);
	for (const auto& Transaction : Transactions) if (!TryApplyDelta(Transaction, OutError)) return false;
	return true;
}

bool FVoxelOverlayReplica::TryApplyDelta(const FGuid& InTransaction, FString& OutError)
{
	if (!bDetachedWorker) return QueuePreparation(InTransaction, true, OutError);
	auto* Pending = Receiving->Transactions.Find(InTransaction);
	if (!Pending || Pending->Parts.Num() != int32(Pending->Count)) return true;
	const auto* First = Pending->Header();
	if (!First || First->Heads.IsEmpty()) return true;
	if (Pending->Phase == 0)
	{
		for (const auto& Head : First->Heads)
		{
			const auto Current = RegionAt(Root, Head.Region);
			if (!Current || Current->Sequence < Head.PreviousSequence) { MarkGap(Head.Region); return true; }
			if (Current->Sequence != Head.PreviousSequence || Current->DirectoryHash != Head.BeforeHash ||
				Head.PreviousSequence == MAX_uint64 || Head.Sequence != Head.PreviousSequence + 1)
				return FailRegion(Head.Region, TEXT("增量前置水位或目录 Hash 不符"), OutError);
		}
		if (!Pending->PreparationLease)
		{
			const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint8>(Pending->TotalBytes) +
				VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe>>(First->Heads.Num()) +
				VoxelBoundedAllocation::MapBytes<FIntVector, bool>(32768) +
				VoxelBoundedAllocation::ArrayBytes<FIntVector>(32768) + sizeof(FVoxelByteReader);
			if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, First->Region, ++NextSerial),
				EVoxelTerrainResourcePool::Working, Bytes, Pending->PreparationLease))
			{
				OutError = TEXT("CapacityBlocked: 增量读取 scratch 尚未准入");
				return true;
			}
			Pending->Assembly.Reserve(Pending->TotalBytes);
			Pending->PreparedRegions.Reserve(First->Heads.Num());
			Pending->SeenSections.Reserve(32768);
			Pending->ChangedSections.Reserve(32768);
		}
		if (!Pending->Assemble(Pending->Assembly, OutError)) return FailRegion(First->Region, MoveTemp(OutError), OutError);
		Pending->Reader = MakeUnique<FVoxelByteReader>(Pending->Assembly);
		Pending->EntryCount = Pending->Reader->U32();
		if (Pending->EntryCount == 0 || Pending->EntryCount > 32768)
			return FailRegion(First->Region, TEXT("完整区域事务条数无效"), OutError);
		Pending->Phase = 1;
		return true;
	}
	if (Pending->Phase == 1)
	{
		const auto& Head = First->Heads[Pending->HeadCursor];
		const auto Previous = RegionAt(Root, Head.Region);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayRegionSnapshot>() +
			VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelOverlayDirectoryEntry>(512) +
			VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(512);
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, Head.Region, ++NextSerial),
			EVoxelTerrainResourcePool::Resident, Bytes, Lease))
		{
			OutError = TEXT("CapacityBlocked: 增量 Region 不可变目录尚未准入");
			return true;
		}
		auto Region = MakeShared<FVoxelOverlayRegionSnapshot, ESPMode::ThreadSafe>();
		Region->ResourceLease = MoveTemp(Lease);
		Region->Region = Previous->Region;
		Region->InitialCoverageTicket = Previous->InitialCoverageTicket;
		Region->DirectoryEpoch = Previous->DirectoryEpoch;
		Region->DirectoryRevision = Head.Sequence;
		Region->Sequence = Head.Sequence;
		Region->AuthorityCut = First->AuthorityCut;
		Region->Directory.Reserve(512);
		Region->Overlays.Reserve(512);
		for (const auto& Pair : Previous->Directory) Region->Directory.Add(Pair.Key, Pair.Value);
		for (const auto& Pair : Previous->Overlays) Region->Overlays.Add(Pair.Key, Pair.Value);
		Pending->PreparedRegions.Add(Head.Region, MoveTemp(Region));
		if (++Pending->HeadCursor == First->Heads.Num()) { Pending->HeadCursor = 0; Pending->Phase = 2; }
		return true;
	}
	if (Pending->Phase == 2)
	{
		if (Pending->EntryCursor == Pending->EntryCount)
		{
			if (!Pending->Reader->End()) return FailRegion(First->Region, TEXT("增量事务包含尾部或截断正文"), OutError);
			Pending->Phase = 3;
			return true;
		}
		// 容量等待不会推进解析游标，也不会留下半个 Section 的目录修改。
		FVoxelByteReader Reader = *Pending->Reader;
		FVoxelOverlayDirectoryEntry Entry;
		if (!ReadEntry(Reader, Entry, true) || Pending->SeenSections.Contains(Entry.Section))
			return FailRegion(First->Region, TEXT("增量 Section 编码无效或重复"), OutError);
		const auto Payload = Reader.BlobView(FVoxelNetworkCodec::MaxRepresentationBytes);
		const auto* Region = Pending->PreparedRegions.Find(FVoxelOverlayRegionCodec::RegionOf(Entry.Section));
		if (!Reader.IsValid() || !Region) return FailRegion(First->Region, TEXT("增量正文越出声明 Region 或截断"), OutError);
		const auto* Before = (*Region)->Directory.Find(Entry.Section);
		if ((Before ? Before->Revision : 0) != Entry.BeforeRevision ||
			(Before ? Before->PayloadHash : FVoxelOverlayHash()) != Entry.BeforeHash || Entry.BeforeRevision == MAX_uint64 ||
			Entry.Revision != Entry.BeforeRevision + 1 || (!Before && (*Region)->Directory.Num() >= 512))
			return FailRegion(First->Region, TEXT("Section 增量前置版本或目录容量不符"), OutError);
		FVoxelOverlayHash Actual;
		if (Payload.Num() != int32(Entry.PayloadBytes) || !FVoxelOverlayHash::Compute(Payload, Actual) || Actual != Entry.PayloadHash)
			return FailRegion(First->Region, TEXT("增量 Section 正文完整性失败"), OutError);
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Decoded;
		if (!Entry.bTombstone)
		{
			uint64 Bytes = 0;
			if (!FVoxelOverlayRegionCodec::OverlayCapacity(Payload, Bytes))
				return FailRegion(First->Region, TEXT("增量 Overlay 容量或编码不合法"), OutError);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, Entry.Section, ++NextSerial),
				EVoxelTerrainResourcePool::Resident, Bytes, Lease))
			{
				OutError = TEXT("CapacityBlocked: 增量 Overlay 正文驻留尚未准入");
				return true;
			}
			auto Overlay = MakeShared<FVoxelOverlaySnapshot, ESPMode::ThreadSafe>();
			if (!FVoxelOverlayRegionCodec::DecodeOverlay(Payload, *Overlay, OutError) ||
				Overlay->Section != Entry.Section || Overlay->Revision != Entry.Revision)
				return FailRegion(First->Region, TEXT("增量 Overlay 正文身份不符"), OutError);
			Overlay->ResourceLease = MoveTemp(Lease);
			Decoded = MoveTemp(Overlay);
		}
		if (Entry.bTombstone) (*Region)->Overlays.Remove(Entry.Section);
		else (*Region)->Overlays.Add(Entry.Section, MoveTemp(Decoded));
		(*Region)->Directory.Add(Entry.Section, Entry);
		Pending->SeenSections.Add(Entry.Section, true);
		Pending->ChangedSections.Add(Entry.Section);
		*Pending->Reader = Reader;
		++Pending->EntryCursor;
		return true;
	}
	if (!Pending->PathLease)
	{
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, First->Region, ++NextSerial),
			EVoxelTerrainResourcePool::Resident, 33 * First->Heads.Num() * RootNodeBytes(), Pending->PathLease, 33 * First->Heads.Num()))
		{
			OutError = TEXT("CapacityBlocked: 增量根路径尚未准入");
			return true;
		}
		Pending->PreparedRoot = Root;
	}
	if (Pending->HeadCursor < First->Heads.Num())
	{
		const auto& Head = First->Heads[Pending->HeadCursor];
		auto Region = Pending->PreparedRegions.FindChecked(Head.Region);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(Region->Directory.Num()) +
			VoxelBoundedAllocation::InitialGrowBytes<uint8>(Region->Directory.Num() * 128 + 4);
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, Head.Region, ++NextSerial),
			EVoxelTerrainResourcePool::Working, Bytes, Scratch))
		{
			OutError = TEXT("CapacityBlocked: 增量目录 Hash scratch 尚未准入");
			return true;
		}
		TArray<FVoxelOverlayDirectoryEntry> Directory;
		Directory.Reserve(Region->Directory.Num());
		Region->Directory.GenerateValueArray(Directory);
		Directory.Sort([](const auto& A, const auto& B) { return LessKey(A.Section, B.Section); });
		if (!FVoxelOverlayRegionCodec::DirectoryHash(Directory, Region->DirectoryHash) || Region->DirectoryHash != Head.Hash)
			return FailRegion(Head.Region, TEXT("增量后的完整目录 Hash 不符"), OutError);
		Pending->PreparedRoot = SetRegion(Pending->PreparedRoot, Region, 31, Pending->PathLease);
		++Pending->HeadCursor;
		return true;
	}
	const auto Notification = PrepareNotification(InTransaction, MoveTemp(Pending->ChangedSections), First->Heads);
	if (!Notification) { OutError = TEXT("CapacityBlocked: 增量提交通知尚未准入"); return true; }
	Root = Pending->PreparedRoot;
	for (const auto& Head : First->Heads)
		Receiving->States.Add(Head.Region, Head.Sequence >= Receiving->RequiredSequences.FindRef(Head.Region) ?
			EVoxelOverlayReplicaState::Complete : EVoxelOverlayReplicaState::Gap);
	Receiving->SettledTransactions.Add(InTransaction, Pending->Hash);
	Receiving->SettledOrder.Add(InTransaction);
	Receiving->Bytes -= Pending->StoredBytes;
	Receiving->Transactions.Remove(InTransaction);
	Receiving->DeltaOrder.Remove(InTransaction);
	if (OnCommitted) OnCommitted(Notification);
	return true;
}
bool FVoxelOverlayReplica::ExpectTicket(const FGuid& InSubscription, const FGuid& InTicket,
	TConstArrayView<FIntVector> InRegions)
{
	check(IsInGameThread());
	if (!InSubscription.IsValid() || !InTicket.IsValid() || InRegions.IsEmpty() ||
		InRegions.Num() > FVoxelOverlayRegionCodec::MaximumRegions || Receiving->Tickets.Num() >= 1024) return false;
	if (const auto* Existing = Receiving->Tickets.Find(InTicket))
	{
		if (Existing->Subscription != InSubscription || Existing->Regions.Num() != InRegions.Num()) return false;
		for (int32 Index = 0; Index < InRegions.Num(); ++Index) if (Existing->Regions[Index] != InRegions[Index]) return false;
		return true;
	}
	FVoxelOverlayReplicaReceiving::FTicket Ticket;
	Ticket.Subscription = InSubscription;
	for (const auto& Region : InRegions)
	{
		if (Ticket.Regions.Contains(Region)) return false;
		Ticket.Regions.Add(Region);
	}
	Receiving->Tickets.Add(InTicket, MoveTemp(Ticket));
	return true;
}

void FVoxelOverlayReplica::CancelTicket(const FGuid& InTicket)
{
	check(IsInGameThread());
	if (Preparing && Preparing->CoverageTicket == InTicket && TaskCanceler) TaskCanceler(Preparing->Stamp);
	if (PendingTask.IsSet() && Preparing && Preparing->CoverageTicket == InTicket) CancelPendingPreparation();
	if (const auto* Ticket = Receiving->Tickets.Find(InTicket); Ticket && Ticket->Cohort)
		Receiving->Bytes -= Ticket->Cohort->AllocatedBytes();
	Receiving->Tickets.Remove(InTicket);
	Receiving->CapacityBlockedTickets.Remove(InTicket);
	Receiving->TicketOrder.Remove(InTicket);
	for (auto Iterator = Receiving->Regions.CreateIterator(); Iterator; ++Iterator)
		if (Iterator.Value().Begin.CoverageTicket == InTicket)
		{
			Receiving->Bytes -= Iterator.Value().StoredBytes;
			Iterator.RemoveCurrent();
		}
}

bool FVoxelOverlayReplica::Advance(const int32 InMaximumOperations, FString& OutError)
{
	check(IsInGameThread());
	if (PendingTask.IsSet() && !RetryPendingPreparation(OutError)) return false;
	for (int32 Work = 0; Work < InMaximumOperations; ++Work)
	{
		if (!Receiving->TicketOrder.IsEmpty() && ((Work & 1) == 0 || Receiving->DeltaOrder.IsEmpty()))
		{
			Receiving->TicketCursor %= Receiving->TicketOrder.Num();
			const FGuid Ticket = Receiving->TicketOrder[Receiving->TicketCursor++];
			const uint64* Blocked = Receiving->CapacityBlockedTickets.Find(Ticket);
			if (!Blocked || Resources->Snapshot().ReleaseVersion > *Blocked)
				if (!TryCommitTicket(Ticket, OutError)) return false;
		}
		else if (!Receiving->DeltaOrder.IsEmpty())
		{
			Receiving->DeltaCursor %= Receiving->DeltaOrder.Num();
			const FGuid Transaction = Receiving->DeltaOrder[Receiving->DeltaCursor++];
			if (!TryApplyDelta(Transaction, OutError)) return false;
		}
		else break;
	}
	return true;
}

bool FVoxelOverlayReplica::Capture(const FVoxelGenerationBounds& InBounds, const FVoxelWorldManifest& InManifest,
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	FVoxelFineOverlayCapture& OutCapture, FString& OutError) const
{
	check(IsInGameThread());
	const auto Consumer = FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, OwnerIncarnation,
		InBounds.Min, FPlatformTime::Cycles64() | 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EnumerationLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(FVoxelOverlayRegionCodec::MaximumRegions), EnumerationLease))
	{
		OutError = TEXT("CapacityBlocked: Replica 区域枚举尚未准入");
		return false;
	}
	TArray<FIntVector> RegionKeys;
	RegionKeys.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
	FVoxelOverlayHash ManifestHash;
	if (!InRegistry || !FVoxelOverlayRegionCodec::ManifestHash(InManifest, ManifestHash) || ManifestHash != ManifestIdentity ||
		!FVoxelOverlayRegionCodec::GatherRegions(InBounds, RegionKeys)) return false;
	FGuid CoverageTicket;
	int32 ModifiedCount = 0;
	for (const auto& Key : RegionKeys)
	{
		const auto* Region = FindRegion(Key);
		if (!Region || GetState(Key) != EVoxelOverlayReplicaState::Complete)
		{
			OutError = TEXT("WaitingOverlay: 请求范围存在未知、缺口或未完成目录");
			return false;
		}
		if (!CoverageTicket.IsValid()) CoverageTicket = Region->InitialCoverageTicket;
		if (CoverageTicket != Region->InitialCoverageTicket)
		{
			OutError = TEXT("WaitingOverlay: 正文与 Halo 尚未形成同一初始 CoverageTicket 的一致切点");
			return false;
		}
		for (const auto& Pair : Region->Overlays)
			ModifiedCount += InBounds.Intersects({Pair.Key * 16, (Pair.Key + FIntVector(1)) * 16}) ? 1 : 0;
	}
	const uint64 Peak = VoxelBoundedAllocation::SharedBytes<FRemoteReadLease>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelOverlaySnapshotSet>() + VoxelBoundedAllocation::ArrayBytes<FRegionPtr>(RegionKeys.Num()) +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(RegionKeys.Num()) +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(ModifiedCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(ModifiedCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Peak, Lease))
	{
		OutError = TEXT("CapacityBlocked: Replica 固定切点描述尚未准入");
		return false;
	}
	auto ReadLease = MakeShared<FRemoteReadLease, ESPMode::ThreadSafe>();
	ReadLease->Epoch = LocalEpoch;
	ReadLease->DirectoryEpoch = DirectoryEpoch;
	ReadLease->Bounds = InBounds;
	ReadLease->ResourceLease = Lease;
	ReadLease->Regions.Reserve(RegionKeys.Num());
	FVoxelFineOverlayCapture Capture;
	Capture.WorldEpoch = LocalEpoch;
	Capture.Bounds = InBounds;
	Capture.Manifest = InManifest;
	Capture.Registry = MoveTemp(InRegistry);
	Capture.ReplicaDirectoryEpoch = DirectoryEpoch;
	Capture.Resources = Resources;
	Capture.Consumer = Consumer;
	Capture.Resident.ResourceLease = MoveTemp(Lease);
	Capture.Resident.PinnedSections.Reserve(ModifiedCount);
	Capture.ContentRevisions.Reserve(ModifiedCount);
	Capture.ReplicaRegionSequences.Reserve(RegionKeys.Num());
	if (!CaptureVersion(InBounds, Capture.VersionProof, OutError)) return false;
	for (const auto& Key : RegionKeys)
	{
		const auto* Node = Root.Get();
		for (int32 Bit = 31; Bit >= 0; --Bit) Node = Node->Children[Branch(Key, Bit)].Get();
		ReadLease->Regions.Add(Node->Region);
		Capture.ReplicaRegionSequences.Add(Key, Node->Region->Sequence);
		for (const auto& Pair : Node->Region->Overlays)
			if (InBounds.Intersects({Pair.Key * 16, (Pair.Key + FIntVector(1)) * 16}))
			{
				Capture.Resident.PinnedSections.Add(Pair.Key, Pair.Value);
				Capture.ContentRevisions.Add(Pair.Key, Pair.Value->Revision);
			}
	}
	Capture.ReadLeaseSet = MoveTemp(ReadLease);
	OutCapture = MoveTemp(Capture);
	OutError.Reset();
	return true;
}
bool FVoxelOverlayReplica::IsCurrent(const FVoxelFineOverlayReadResult& InRead) const
{
	if (InRead.WorldEpoch != LocalEpoch || !InRead.ReadLeaseSet || !InRead.ReadLeaseSet->IsRemoteDirectory() ||
		InRead.ReplicaDirectoryEpoch != DirectoryEpoch || InRead.ReplicaRegionSequences.IsEmpty()) return false;
	for (const auto& Pair : InRead.ReplicaRegionSequences)
	{
		const auto* Region = FindRegion(Pair.Key);
		if (!Region || Region->Sequence != Pair.Value || GetState(Pair.Key) != EVoxelOverlayReplicaState::Complete) return false;
	}
	return true;
}

bool FVoxelOverlayReplica::CaptureVersion(const FVoxelGenerationBounds& InBounds,
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe>& OutProof, FString& OutError) const
{
	check(IsInGameThread());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Peak = VoxelBoundedAllocation::SharedBytes<FVoxelFineOverlayVersionProof>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(FVoxelOverlayRegionCodec::MaximumRegions) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(FVoxelOverlayRegionCodec::MaximumRegions);
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1, InBounds.Min,
		FPlatformTime::Cycles64() | 1), EVoxelTerrainResourcePool::Resident, Peak, Lease)) return false;
	TArray<FIntVector> Regions;
	if (!FVoxelOverlayRegionCodec::GatherRegions(InBounds, Regions) || Regions.IsEmpty()) return false;
	auto Proof = MakeShared<FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe>();
	Proof->WorldEpoch = LocalEpoch;
	Proof->DirectoryEpoch = DirectoryEpoch;
	Proof->RegionSequences.Reserve(Regions.Num());
	Proof->ResourceLease = Lease;
	FGuid Ticket;
	for (const auto& Key : Regions)
	{
		const auto* Region = FindRegion(Key);
		if (!Region || GetState(Key) != EVoxelOverlayReplicaState::Complete ||
			(Ticket.IsValid() && Ticket != Region->InitialCoverageTicket))
		{
			OutError = TEXT("WaitingOverlay: 正文与 Halo 未形成完整连续的同一切点");
			return false;
		}
		Ticket = Region->InitialCoverageTicket;
		Proof->RegionSequences.Add(Key, Region->Sequence);
	}
	Regions.Empty();
	Lease->Resize(VoxelBoundedAllocation::SharedBytes<FVoxelFineOverlayVersionProof>() + Proof->RegionSequences.GetAllocatedSize());
	OutProof = MoveTemp(Proof);
	OutError.Reset();
	return true;
}

bool FVoxelOverlayReplica::IsCurrent(const FVoxelFineOverlayVersionProof& InProof) const
{
	if (InProof.WorldEpoch != LocalEpoch || InProof.DirectoryEpoch != DirectoryEpoch || InProof.RegionSequences.IsEmpty()) return false;
	for (const auto& Pair : InProof.RegionSequences)
	{
		const auto* Region = FindRegion(Pair.Key);
		if (!Region || Region->Sequence != Pair.Value || GetState(Pair.Key) != EVoxelOverlayReplicaState::Complete) return false;
	}
	return true;
}

bool FVoxelOverlayReplica::LookupSection(const FIntVector& InSection, uint64& OutRevision,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& OutOverlay) const
{
	const FIntVector Key = FVoxelOverlayRegionCodec::RegionOf(InSection);
	const auto* Region = FindRegion(Key);
	if (!Region || GetState(Key) != EVoxelOverlayReplicaState::Complete) return false;
	const auto* Entry = Region->Directory.Find(InSection);
	OutRevision = Entry ? Entry->Revision : 0;
	OutOverlay = Region->Overlays.FindRef(InSection);
	return !Entry || Entry->bTombstone || OutOverlay.IsValid();
}

TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> FVoxelOverlayReplica::PrepareNotification(
	const FGuid& InTransaction, TArray<FIntVector>&& InSections, TConstArrayView<FVoxelOverlayRegionHead> InHeads)
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayCommitNotification>() +
		InSections.GetAllocatedSize() + VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(InHeads.Num()) + 128;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(LocalEpoch, OwnerId, 1,
		InHeads[0].Region, ++NextSerial), EVoxelTerrainResourcePool::Resident, Bytes, Lease)) return nullptr;
	auto Notification = MakeShared<FVoxelOverlayCommitNotification, ESPMode::ThreadSafe>();
	Notification->AuthorityTransaction = InTransaction;
	Notification->Sections = MoveTemp(InSections);
	Notification->Heads.Reserve(InHeads.Num());
	Notification->Heads.Append(InHeads.GetData(), InHeads.Num());
	Notification->ResourceLease = MoveTemp(Lease);
	return Notification;
}

void FVoxelOverlayReplica::Reset()
{
	check(OwnerIncarnation < MAX_uint64);
	if (Preparing && TaskCanceler) TaskCanceler(Preparing->Stamp);
	++OwnerIncarnation;
	PendingTask.Reset();
	bBindingCapacityBlocked = false;
	Preparing.Reset();
	Root.Reset();
	Receiving = MakeUnique<FVoxelOverlayReplicaReceiving>();
}

uint64 FVoxelOverlayReplica::GetAuthorityEpoch() const { return AuthorityEpoch; }
uint64 FVoxelOverlayReplica::GetProgressSerial() const { return ProgressSerial; }
const FGuid& FVoxelOverlayReplica::GetDirectoryEpoch() const { return DirectoryEpoch; }
const FVoxelOverlayHash& FVoxelOverlayReplica::GetManifestIdentity() const { return ManifestIdentity; }
