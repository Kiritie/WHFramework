#pragma once
#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
enum class EVoxelTerrainResourceAdmission : uint8;
struct FVoxelGenerationConsumerKey;

// 字节容器先释放，实际输出租约随后归还；禁止隐式复制正文。
struct WHFRAMEWORK_API FVoxelBinaryBuffer
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TArray<uint8> Bytes;

	FVoxelBinaryBuffer() = default;
	FVoxelBinaryBuffer(FVoxelBinaryBuffer&&) = default;
	FVoxelBinaryBuffer& operator=(FVoxelBinaryBuffer&& InOther)
	{
		if (this == &InOther) return *this;
		Bytes = MoveTemp(InOther.Bytes);
		ResourceLease = MoveTemp(InOther.ResourceLease);
		return *this;
	}
	FVoxelBinaryBuffer(const FVoxelBinaryBuffer&) = delete;
	FVoxelBinaryBuffer& operator=(const FVoxelBinaryBuffer&) = delete;
};
class WHFRAMEWORK_API FVoxelByteWriter
{
public:
    explicit FVoxelByteWriter(int32 InLimit=1024*1024):Limit(InLimit){}
	bool Reserve(int32 InBytes);
    void U8(uint8 V);void U16(uint16 V);void U32(uint32 V);void U64(uint64 V);
    void I32(int32 V){U32(uint32(V));}
    void Guid(const FGuid& G);
    void String(const FString& S,int32 MaxBytes=1024);
    void Blob(TConstArrayView<uint8> V,int32 MaxBytes=256*1024);
    void Raw(TConstArrayView<uint8> V);
    bool Finish(TArray<uint8>& Out);
    bool IsValid()const{return bOK;}
private:
    TArray<uint8> Data;int32 Limit;bool bOK=true;
};
class WHFRAMEWORK_API FVoxelByteReader
{
public:
    explicit FVoxelByteReader(TConstArrayView<uint8> In):Data(In){}
    uint8 U8();uint16 U16();uint32 U32();uint64 U64();int32 I32(){return int32(U32());}
    FGuid Guid();FString String(int32 MaxBytes=1024);TArray<uint8> Blob(int32 MaxBytes=256*1024);
    bool Raw(int32 Num,TArray<uint8>& Out);
	TConstArrayView<uint8> View(int32 InBytes);
	TConstArrayView<uint8> BlobView(int32 InMaximumBytes);
	TConstArrayView<uint8> StringView(int32 InMaximumBytes = 1024);
    bool End()const{return bOK&&Pos==Data.Num();}
    bool IsValid()const{return bOK;}
    void Reject(){bOK=false;}
    int32 Remaining()const{return Data.Num()-Pos;}
private:
    TConstArrayView<uint8> Data;int32 Pos=0;bool bOK=true;
};
namespace VoxelBinary
{
    WHFRAMEWORK_API uint64 Hash(TConstArrayView<uint8> Bytes);
    WHFRAMEWORK_API uint64 Mix64(uint64 V);
	WHFRAMEWORK_API int32 CompressionBound(int32 InRawBytes);
    WHFRAMEWORK_API bool Compress(TConstArrayView<uint8> Raw,TArray<uint8>& Out,int32 Limit=1024*1024, int32 InRawLimit = 0);
    WHFRAMEWORK_API bool Decompress(TConstArrayView<uint8> Stored,TArray<uint8>& Out,int32 Limit=1024*1024);
	WHFRAMEWORK_API bool CompressGranted(TConstArrayView<uint8> InRaw, FVoxelBinaryBuffer& OutBuffer,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, int32 InRawLimit, int32 InStoredLimit,
		bool& OutCapacityBlocked, EVoxelTerrainResourceAdmission* OutAdmission = nullptr);
	// OutRaw 的正文 backing 由调用方提前授予并保留；此入口另授予每次原生 inflate 分配。
	WHFRAMEWORK_API bool DecompressGranted(TConstArrayView<uint8> InStored, TArray<uint8>& OutRaw,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, int32 InRawLimit, bool& OutCapacityBlocked,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr);
}
