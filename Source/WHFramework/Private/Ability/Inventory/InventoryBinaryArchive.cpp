#include "Ability/Inventory/InventoryBinaryArchive.h"

#include "Containers/StringConv.h"

FInventoryBinaryWriter::FInventoryBinaryWriter(int32 InLimit)
    : Limit(InLimit)
{
    bValid = Limit > 0;
}

void FInventoryBinaryWriter::U8(uint8 InValue)
{
    if(!bValid || Bytes.Num() >= Limit)
    {
        bValid = false;
        return;
    }
    Bytes.Add(InValue);
}

void FInventoryBinaryWriter::U32(uint32 InValue)
{
    for(int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
    {
        U8(static_cast<uint8>(InValue >> (ByteIndex * 8)));
    }
}

void FInventoryBinaryWriter::U64(uint64 InValue)
{
    for(int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
    {
        U8(static_cast<uint8>(InValue >> (ByteIndex * 8)));
    }
}

void FInventoryBinaryWriter::Guid(const FGuid& InValue)
{
    U32(InValue.A);
    U32(InValue.B);
    U32(InValue.C);
    U32(InValue.D);
}

void FInventoryBinaryWriter::String(const FString& InValue, int32 InMaximumBytes)
{
    const FTCHARToUTF8 Encoded(*InValue);
    if(Encoded.Length() > InMaximumBytes)
    {
        bValid = false;
        return;
    }
    U32(static_cast<uint32>(Encoded.Length()));
    Raw(MakeArrayView(reinterpret_cast<const uint8*>(Encoded.Get()), Encoded.Length()));
}

void FInventoryBinaryWriter::Name(FName InValue, int32 InMaximumBytes)
{
    String(InValue.IsNone() ? FString() : InValue.ToString(), InMaximumBytes);
}

void FInventoryBinaryWriter::Blob(TConstArrayView<uint8> InValue, int32 InMaximumBytes)
{
    if(InValue.Num() > InMaximumBytes)
    {
        bValid = false;
        return;
    }
    U32(static_cast<uint32>(InValue.Num()));
    Raw(InValue);
}

void FInventoryBinaryWriter::Raw(TConstArrayView<uint8> InValue)
{
    if(!bValid || InValue.Num() > Limit - Bytes.Num())
    {
        bValid = false;
        return;
    }
    Bytes.Append(InValue.GetData(), InValue.Num());
}

bool FInventoryBinaryWriter::IsValid() const
{
    return bValid;
}

const TArray<uint8>& FInventoryBinaryWriter::GetBytes() const
{
    return Bytes;
}

TArray<uint8> FInventoryBinaryWriter::TakeBytes()
{
    return MoveTemp(Bytes);
}

FInventoryBinaryReader::FInventoryBinaryReader(TConstArrayView<uint8> InBytes)
    : Bytes(InBytes)
{
}

uint8 FInventoryBinaryReader::U8()
{
    if(!bValid || Position >= Bytes.Num())
    {
        bValid = false;
        return 0;
    }
    return Bytes[Position++];
}

uint32 FInventoryBinaryReader::U32()
{
    uint32 Value = 0;
    for(int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
    {
        Value |= static_cast<uint32>(U8()) << (ByteIndex * 8);
    }
    return Value;
}

uint64 FInventoryBinaryReader::U64()
{
    uint64 Value = 0;
    for(int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
    {
        Value |= static_cast<uint64>(U8()) << (ByteIndex * 8);
    }
    return Value;
}

FGuid FInventoryBinaryReader::Guid()
{
    const uint32 PartA = U32();
    const uint32 PartB = U32();
    const uint32 PartC = U32();
    const uint32 PartD = U32();
    return FGuid(PartA, PartB, PartC, PartD);
}

FString FInventoryBinaryReader::String(int32 InMaximumBytes)
{
    const uint32 Count = U32();
    if(!bValid || Count > static_cast<uint32>(InMaximumBytes) || Count > static_cast<uint32>(Remaining()))
    {
        Reject();
        return FString();
    }
    const TConstArrayView<uint8> Encoded = Raw(static_cast<int32>(Count));
    if(Count == 0)
    {
        return FString();
    }
    for(uint8 Byte : Encoded)
    {
        if(Byte == 0)
        {
            Reject();
            return FString();
        }
    }
    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Encoded.GetData()), Encoded.Num());
    const FString Value(Converted.Length(), Converted.Get());
    const FTCHARToUTF8 RoundTrip(*Value);
    if(RoundTrip.Length() != Encoded.Num() || FMemory::Memcmp(RoundTrip.Get(), Encoded.GetData(), Encoded.Num()) != 0)
    {
        Reject();
        return FString();
    }
    return Value;
}

FName FInventoryBinaryReader::Name(int32 InMaximumBytes)
{
    const FString Value = String(InMaximumBytes);
    return Value.IsEmpty() ? NAME_None : FName(*Value);
}

TArray<uint8> FInventoryBinaryReader::Blob(int32 InMaximumBytes)
{
    const uint32 Count = U32();
    if(!bValid || Count > static_cast<uint32>(InMaximumBytes) || Count > static_cast<uint32>(Remaining()))
    {
        Reject();
        return TArray<uint8>();
    }
    const TConstArrayView<uint8> View = Raw(static_cast<int32>(Count));
    TArray<uint8> Value;
    Value.Append(View.GetData(), View.Num());
    return Value;
}

TConstArrayView<uint8> FInventoryBinaryReader::Raw(int32 InCount)
{
    if(!bValid || InCount < 0 || InCount > Remaining())
    {
        Reject();
        return TConstArrayView<uint8>();
    }
    const TConstArrayView<uint8> Result = Bytes.Slice(Position, InCount);
    Position += InCount;
    return Result;
}

bool FInventoryBinaryReader::IsValid() const
{
    return bValid;
}

bool FInventoryBinaryReader::AtEnd() const
{
    return bValid && Position == Bytes.Num();
}

int32 FInventoryBinaryReader::Remaining() const
{
    return Bytes.Num() - Position;
}

void FInventoryBinaryReader::Reject()
{
    bValid = false;
}
