#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Misc/Compression.h"
#include "Misc/Crc.h"
#include "Containers/StringConv.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "zlib.h"

namespace
{
	struct FVoxelZlibAllocation
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	};

	struct FVoxelZlibAllocator
	{
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources;
		const FVoxelGenerationConsumerKey& Consumer;
		bool bCapacityBlocked = false;
		static constexpr uint32 Alignment = 16;
		static constexpr SIZE_T HeaderBytes = Align(sizeof(FVoxelZlibAllocation), Alignment);

		static voidpf Allocate(voidpf InOpaque, uInt InCount, uInt InElementBytes)
		{
			auto& Allocator = *static_cast<FVoxelZlibAllocator*>(InOpaque);
			const uint64 PayloadBytes = uint64(InCount) * InElementBytes;
			if (PayloadBytes == 0 || PayloadBytes > MAX_int32) return Z_NULL;
			const SIZE_T Bytes = FMemory::QuantizeSize(HeaderBytes + SIZE_T(PayloadBytes), Alignment);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (Allocator.Resources && !Allocator.Resources->Acquire(Allocator.Consumer,
				EVoxelTerrainResourcePool::Working, Bytes, Lease))
			{
				Allocator.bCapacityBlocked = true;
				return Z_NULL;
			}
			void* Allocation = FMemory::Malloc(Bytes, Alignment);
			if (!Allocation) return Z_NULL;
			new (Allocation) FVoxelZlibAllocation{MoveTemp(Lease)};
			void* Payload = static_cast<uint8*>(Allocation) + HeaderBytes;
			FMemory::Memzero(Payload, PayloadBytes);
			return Payload;
		}

		static void Free(voidpf InOpaque, voidpf InPayload)
		{
			if (!InPayload) return;
			auto* Allocation = reinterpret_cast<FVoxelZlibAllocation*>(static_cast<uint8*>(InPayload) - HeaderBytes);
			auto Lease = MoveTemp(Allocation->ResourceLease);
			Allocation->~FVoxelZlibAllocation();
			FMemory::Free(Allocation);
			Lease.Reset();
		}
	};
}
bool FVoxelByteWriter::Reserve(const int32 InBytes)
{
	if (!bOK || InBytes < Data.Num() || InBytes > Limit) { bOK = false; return false; }
	Data.Reserve(InBytes); return true;
}
void FVoxelByteWriter::U8(uint8 V){if(!bOK||Data.Num()>=Limit){bOK=false;return;}Data.Add(V);}
void FVoxelByteWriter::U16(uint16 V){for(int32 I=0;I<2;++I)U8(uint8(V>>(8*I)));}
void FVoxelByteWriter::U32(uint32 V){for(int32 I=0;I<4;++I)U8(uint8(V>>(8*I)));}
void FVoxelByteWriter::U64(uint64 V){for(int32 I=0;I<8;++I)U8(uint8(V>>(8*I)));}
void FVoxelByteWriter::Guid(const FGuid& G){U32(G.A);U32(G.B);U32(G.C);U32(G.D);}
void FVoxelByteWriter::Raw(TConstArrayView<uint8> V)
{
    if(!bOK||V.Num()>Limit-Data.Num()){bOK=false;return;}
    if(V.Num())Data.Append(V.GetData(),V.Num());
}
void FVoxelByteWriter::String(const FString& S,int32 Max)
{
    FTCHARToUTF8 V(*S);if(V.Length()>Max){bOK=false;return;}
    U32(uint32(V.Length()));Raw(MakeArrayView(reinterpret_cast<const uint8*>(V.Get()),V.Length()));
}
void FVoxelByteWriter::Blob(TConstArrayView<uint8> V,int32 Max)
{if(V.Num()>Max){bOK=false;return;}U32(uint32(V.Num()));Raw(V);}
bool FVoxelByteWriter::Finish(TArray<uint8>& O){if(!bOK)return false;O=MoveTemp(Data);return true;}
uint8 FVoxelByteReader::U8(){if(!bOK||Pos>=Data.Num()){bOK=false;return 0;}return Data[Pos++];}
uint16 FVoxelByteReader::U16(){uint16 V=0;for(int32 I=0;I<2;++I)V|=uint16(U8())<<(8*I);return V;}
uint32 FVoxelByteReader::U32(){uint32 V=0;for(int32 I=0;I<4;++I)V|=uint32(U8())<<(8*I);return V;}
uint64 FVoxelByteReader::U64(){uint64 V=0;for(int32 I=0;I<8;++I)V|=uint64(U8())<<(8*I);return V;}
FGuid FVoxelByteReader::Guid(){uint32 A=U32(),B=U32(),C=U32(),D=U32();return FGuid(A,B,C,D);}
bool FVoxelByteReader::Raw(int32 N,TArray<uint8>& O)
{
    if(!bOK||N<0||N>Remaining()){bOK=false;return false;}
    TArray<uint8>T;T.Reserve(N);if(N)T.Append(Data.GetData()+Pos,N);Pos+=N;O=MoveTemp(T);return true;
}
TConstArrayView<uint8> FVoxelByteReader::View(const int32 InBytes)
{
	if (!bOK || InBytes < 0 || InBytes > Remaining())
	{
		Reject();
		return {};
	}
	const auto Result = Data.Slice(Pos, InBytes);
	Pos += InBytes;
	return Result;
}

TConstArrayView<uint8> FVoxelByteReader::BlobView(const int32 InMaximumBytes)
{
	const uint32 Size = U32();
	if (InMaximumBytes < 0 || Size > uint32(InMaximumBytes))
	{
		Reject();
		return {};
	}
	return View(int32(Size));
}

TConstArrayView<uint8> FVoxelByteReader::StringView(const int32 InMaximumBytes)
{
	const auto Bytes = BlobView(InMaximumBytes);
	for (int32 Index = 0; IsValid() && Index < Bytes.Num();)
	{
		const uint8 Lead = Bytes[Index++];
		if (Lead == 0) { Reject(); break; }
		if (Lead < 0x80) continue;
		const int32 TailCount = Lead >= 0xc2 && Lead <= 0xdf ? 1 :
			Lead >= 0xe0 && Lead <= 0xef ? 2 : Lead >= 0xf0 && Lead <= 0xf4 ? 3 : 0;
		if (TailCount == 0 || TailCount > Bytes.Num() - Index) { Reject(); break; }
		const uint8 FirstTail = Bytes[Index];
		if ((Lead == 0xe0 && FirstTail < 0xa0) || (Lead == 0xed && FirstTail >= 0xa0) ||
			(Lead == 0xf0 && FirstTail < 0x90) || (Lead == 0xf4 && FirstTail >= 0x90))
		{
			Reject();
			break;
		}
		for (int32 Tail = 0; Tail < TailCount; ++Tail)
		{
			if ((Bytes[Index++] & 0xc0) != 0x80) { Reject(); break; }
		}
	}
	return IsValid() ? Bytes : TConstArrayView<uint8>();
}
TArray<uint8> FVoxelByteReader::Blob(int32 Max)
{
    uint32 N=U32();TArray<uint8>O;if(N>uint32(Max)){Reject();return O;}Raw(int32(N),O);return O;
}
FString FVoxelByteReader::String(int32 Max)
{
    TArray<uint8>B=Blob(Max);if(!bOK||B.IsEmpty())return FString();
    if(B.Contains(0)){Reject();return FString();}
    FUTF8ToTCHAR C(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());
    FString S(C.Length(),C.Get());FTCHARToUTF8 Back(*S);
    if(Back.Length()!=B.Num()||FMemory::Memcmp(Back.Get(),B.GetData(),B.Num())){Reject();return FString();}
    return S;
}
uint64 VoxelBinary::Hash(TConstArrayView<uint8>B)
{
    uint64 H=14695981039346656037ull;for(uint8 V:B){H^=V;H*=1099511628211ull;}return H;
}
uint64 VoxelBinary::Mix64(uint64 V)
{
    V+=0x9e3779b97f4a7c15ull;V=(V^(V>>30))*0xbf58476d1ce4e5b9ull;
    V=(V^(V>>27))*0x94d049bb133111ebull;return V^(V>>31);
}

int32 VoxelBinary::CompressionBound(const int32 InRawBytes)
{
	if (InRawBytes <= 0) return 0;
	const uLong Bound = compressBound(uLong(InRawBytes));
	return Bound > uLong(MAX_int32 - 16) ? 0 : int32(Bound);
}
bool VoxelBinary::Compress(TConstArrayView<uint8>R,TArray<uint8>&O,int32 Limit, const int32 InRawLimit)
{
	const int32 RawLimit = InRawLimit > 0 ? InRawLimit : Limit;
	const int64 StoredLimit = InRawLimit > 0 ? int64(Limit) : int64(Limit) + 65536 + 16;
	if (Limit <= 0 || RawLimit <= 0 || StoredLimit > MAX_int32 || R.Num() <= 0 || R.Num() > RawLimit) return false;
    int32 N=FCompression::CompressMemoryBound(NAME_Zlib,R.Num());
	if (N <= 0 || int64(N) > int64(RawLimit) + 65536) return false;
	TArray<uint8>C; C.Reserve(N); C.SetNumUninitialized(N);
    if(!FCompression::CompressMemory(NAME_Zlib,C.GetData(),N,R.GetData(),R.Num()))return false;
	if (int64(N) + 16 > StoredLimit) return false;
	C.SetNum(N, EAllowShrinking::No); FVoxelByteWriter W{int32(StoredLimit)}; W.Reserve(N + 16);
    W.U32(0x315a5856);W.U32(uint32(R.Num()));W.U32(FCrc::MemCrc32(R.GetData(),R.Num()));
	W.Blob(C, int32(StoredLimit) - 16); return W.Finish(O);
}
bool VoxelBinary::Decompress(TConstArrayView<uint8>S,TArray<uint8>&O,int32 Limit)
{
    if(S.Num()>Limit+65536+16)return false;
    FVoxelByteReader R(S);uint32 HeaderMagic=R.U32(),N=R.U32(),CRC=R.U32();auto C=R.BlobView(Limit+65536);
    if(!R.End()||HeaderMagic!=0x315a5856||N==0||N>uint32(Limit)||C.IsEmpty())return false;
    TArray<uint8>T;T.Reserve(int32(N));T.SetNumUninitialized(int32(N));
    if(!FCompression::UncompressMemory(NAME_Zlib,T.GetData(),T.Num(),C.GetData(),C.Num())||
        FCrc::MemCrc32(T.GetData(),T.Num())!=CRC)return false;
    O=MoveTemp(T);return true;
}

bool VoxelBinary::CompressGranted(const TConstArrayView<uint8> InRaw, FVoxelBinaryBuffer& OutBuffer,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const int32 InRawLimit, const int32 InStoredLimit,
	bool& OutCapacityBlocked)
{
	OutCapacityBlocked = false;
	if (InRawLimit <= 0 || InStoredLimit <= 16 || InRaw.IsEmpty() || InRaw.Num() > InRawLimit) return false;
	const int32 Bound = CompressionBound(InRaw.Num());
	if (Bound <= 0) return false;
	// 原始正文可远大于线上压缩上限；是否能装入以实际 deflate 结果为准。
	const int32 OutputCapacity = FMath::Min(Bound, InStoredLimit - 16);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	FVoxelBinaryBuffer Candidate;
	if (InResources && (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<uint8>(OutputCapacity), ScratchLease) ||
		!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::ArrayBytes<uint8>(OutputCapacity + 16), Candidate.ResourceLease)))
	{
		OutCapacityBlocked = true;
		return false;
	}
	TArray<uint8> Compressed;
	Compressed.Reserve(OutputCapacity);
	Compressed.SetNumUninitialized(OutputCapacity);
	FVoxelZlibAllocator Allocator{InResources, InConsumer};
	z_stream Stream{};
	Stream.zalloc = FVoxelZlibAllocator::Allocate;
	Stream.zfree = FVoxelZlibAllocator::Free;
	Stream.opaque = &Allocator;
	int Result = deflateInit(&Stream, Z_DEFAULT_COMPRESSION);
	if (Result == Z_OK)
	{
		Stream.next_in = const_cast<Bytef*>(InRaw.GetData());
		Stream.avail_in = uInt(InRaw.Num());
		Stream.next_out = Compressed.GetData();
		Stream.avail_out = uInt(OutputCapacity);
		Result = deflate(&Stream, Z_FINISH);
		const int EndResult = deflateEnd(&Stream);
		if (EndResult != Z_OK) Result = EndResult;
	}
	OutCapacityBlocked = Allocator.bCapacityBlocked;
	if (Result != Z_STREAM_END || Stream.total_out > uLong(OutputCapacity) || Stream.total_in != uLong(InRaw.Num())) return false;
	Compressed.SetNum(int32(Stream.total_out), EAllowShrinking::No);
	FVoxelByteWriter Writer(InStoredLimit);
	if (!Writer.Reserve(OutputCapacity + 16)) return false;
	Writer.U32(0x315a5856);
	Writer.U32(uint32(InRaw.Num()));
	Writer.U32(FCrc::MemCrc32(InRaw.GetData(), InRaw.Num()));
	Writer.Blob(Compressed, InStoredLimit - 16);
	if (!Writer.Finish(Candidate.Bytes)) return false;
	check(!Candidate.ResourceLease || Candidate.Bytes.GetAllocatedSize() <= Candidate.ResourceLease->Bytes());
	OutBuffer = MoveTemp(Candidate);
	return true;
}

bool VoxelBinary::DecompressGranted(const TConstArrayView<uint8> InStored, TArray<uint8>& OutRaw,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const int32 InRawLimit, bool& OutCapacityBlocked)
{
	OutCapacityBlocked = false;
	if (InRawLimit <= 0 || InRawLimit > MAX_int32 - 65536 ||
		int64(InStored.Num()) > int64(InRawLimit) + 65536 + 16) return false;
	FVoxelByteReader Header(InStored);
	const uint32 Magic = Header.U32();
	const uint32 RawBytes = Header.U32();
	const uint32 Crc = Header.U32();
	const auto Compressed = Header.BlobView(InRawLimit + 65536);
	if (!Header.End() || Magic != 0x315a5856 || !RawBytes || RawBytes > uint32(InRawLimit) || Compressed.IsEmpty()) return false;
	OutRaw.Reserve(int32(RawBytes));
	OutRaw.SetNumUninitialized(int32(RawBytes));
	FVoxelZlibAllocator Allocator{InResources, InConsumer};
	z_stream Stream{};
	Stream.zalloc = FVoxelZlibAllocator::Allocate;
	Stream.zfree = FVoxelZlibAllocator::Free;
	Stream.opaque = &Allocator;
	int Result = inflateInit(&Stream);
	if (Result == Z_OK)
	{
		Stream.next_in = const_cast<Bytef*>(Compressed.GetData());
		Stream.avail_in = uInt(Compressed.Num());
		Stream.next_out = OutRaw.GetData();
		Stream.avail_out = uInt(RawBytes);
		Result = inflate(&Stream, Z_FINISH);
		const int EndResult = inflateEnd(&Stream);
		if (EndResult != Z_OK) Result = EndResult;
	}
	OutCapacityBlocked = Allocator.bCapacityBlocked;
	return Result == Z_STREAM_END && Stream.total_out == RawBytes &&
		Stream.total_in == uLong(Compressed.Num()) && FCrc::MemCrc32(OutRaw.GetData(), OutRaw.Num()) == Crc;
}
