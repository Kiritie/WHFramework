#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
class WHFRAMEWORK_API FVoxelByteWriter
{
public:
	explicit FVoxelByteWriter(int32 InLimit = 1024 * 1024)
	    : Limit(InLimit)
	{
	}
	void U8(uint8 V);
	void U16(uint16 V);
	void U32(uint32 V);
	void U64(uint64 V);
	void I32(int32 V)
	{
		U32(uint32(V));
	}
	void Guid(const FGuid& G);
	void String(const FString& S, int32 MaxBytes = 1024);
	void Blob(TConstArrayView<uint8> V, int32 MaxBytes = 256 * 1024);
	void Raw(TConstArrayView<uint8> V);
	bool Finish(TArray<uint8>& Out);
	bool IsValid() const
	{
		return bOK;
	}

private:
	TArray<uint8> Data;
	int32 Limit;
	bool bOK = true;
};
class WHFRAMEWORK_API FVoxelByteReader
{
public:
	explicit FVoxelByteReader(TConstArrayView<uint8> In)
	    : Data(In)
	{
	}
	uint8 U8();
	uint16 U16();
	uint32 U32();
	uint64 U64();
	int32 I32()
	{
		return int32(U32());
	}
	FGuid Guid();
	FString String(int32 MaxBytes = 1024);
	TArray<uint8> Blob(int32 MaxBytes = 256 * 1024);
	bool Raw(int32 Num, TArray<uint8>& Out);
	bool End() const
	{
		return bOK && Pos == Data.Num();
	}
	bool IsValid() const
	{
		return bOK;
	}
	void Reject()
	{
		bOK = false;
	}
	int32 Remaining() const
	{
		return Data.Num() - Pos;
	}

private:
	TConstArrayView<uint8> Data;
	int32 Pos = 0;
	bool bOK = true;
};
namespace VoxelBinary
{
WHFRAMEWORK_API uint64 Hash(TConstArrayView<uint8> Bytes);
WHFRAMEWORK_API uint64 Mix64(uint64 V);
WHFRAMEWORK_API bool Compress(TConstArrayView<uint8> Raw, TArray<uint8>& Out, int32 Limit = 1024 * 1024);
WHFRAMEWORK_API bool Decompress(TConstArrayView<uint8> Stored, TArray<uint8>& Out, int32 Limit = 1024 * 1024);
} // namespace VoxelBinary
