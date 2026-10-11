#include "Voxel/Save/InventoryVoxelEntityCodec.h"

#include "Misc/Crc.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace InventoryVoxelEnvelope
{
    constexpr uint32 Magic = 0x32584549;
    constexpr uint8 Uninitialized = 0;
    constexpr uint8 Full = 1;
    constexpr uint8 Public = 2;
    constexpr uint8 PublicBytes[8] = {0x49, 0x45, 0x58, 0x32, 2, 0, 2, 0};

    bool Read(const FVoxelBlockEntityState& InState, uint8& OutMode, TConstArrayView<uint8>& OutBody)
    {
        if(InState.Kind != FInventoryVoxelEntityCodec::Kind || InState.Schema != FInventoryVoxelEntityCodec::Schema ||
            InState.Payload.Num() < 8 || InState.Payload.Num() > FInventoryVoxelEntityCodec::MaximumBytes)
        {
            return false;
        }
        FVoxelByteReader Reader(InState.Payload);
        if(Reader.U32() != Magic || Reader.U16() != FInventoryVoxelEntityCodec::Schema)
        {
            return false;
        }
        OutMode = Reader.U8();
        if(Reader.U8() != 0 || OutMode > Public)
        {
            return false;
        }
        if(OutMode != Full)
        {
            OutBody = {};
            return Reader.End();
        }
        OutBody = Reader.BlobView(FInventorySnapshotCodec::MaximumBytes);
        const uint32 Checksum = Reader.U32();
        return Reader.End() && OutBody.Num() >= 64 && FCrc::MemCrc32(OutBody.GetData(), OutBody.Num()) == Checksum;
    }
}

bool FInventoryVoxelEntityCodec::Register()
{
    return FVoxelBlockEntityCodec::Register(Kind, MakeDefault, Validate);
}

bool FInventoryVoxelEntityCodec::IsInventory(const FVoxelBlockEntityState& InState)
{
    return InState.Kind == Kind;
}

bool FInventoryVoxelEntityCodec::IsUninitialized(const FVoxelBlockEntityState& InState)
{
    uint8 Mode = 255;
    TConstArrayView<uint8> Body;
    return InventoryVoxelEnvelope::Read(InState, Mode, Body) && Mode == InventoryVoxelEnvelope::Uninitialized;
}

bool FInventoryVoxelEntityCodec::IsPublic(const FVoxelBlockEntityState& InState)
{
    uint8 Mode = 255;
    TConstArrayView<uint8> Body;
    return InventoryVoxelEnvelope::Read(InState, Mode, Body) && Mode == InventoryVoxelEnvelope::Public;
}

bool FInventoryVoxelEntityCodec::MakeDefault(FVoxelBlockEntityState& OutState, uint8 InVariant)
{
    if(InVariant != 0)
    {
        return false;
    }
    MakePublic(OutState);
    OutState.Payload[6] = InventoryVoxelEnvelope::Uninitialized;
    return true;
}

void FInventoryVoxelEntityCodec::MakePublic(FVoxelBlockEntityState& OutState)
{
    OutState.Kind = Kind;
    OutState.Schema = Schema;
    OutState.Payload.Reset(8);
    OutState.Payload.Append(InventoryVoxelEnvelope::PublicBytes, 8);
}

TConstArrayView<uint8> FInventoryVoxelEntityCodec::GetPublicPayload()
{
    return MakeArrayView(InventoryVoxelEnvelope::PublicBytes);
}

bool FInventoryVoxelEntityCodec::Validate(const FVoxelBlockEntityState& InState)
{
    uint8 Mode = 255;
    TConstArrayView<uint8> Body;
    return InventoryVoxelEnvelope::Read(InState, Mode, Body);
}

bool FInventoryVoxelEntityCodec::Encode(const FInventorySnapshot& InSnapshot, const FInventoryItemRegistry& InRegistry,
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FVoxelBlockEntityState& OutState, FString& OutError)
{
    check(IsInGameThread());
    TArray<uint8> Body;
    if(!FInventorySnapshotCodec::Encode(InSnapshot, InRegistry, InProfiles, Body, OutError))
    {
        return false;
    }
    FVoxelByteWriter Writer(MaximumBytes);
    Writer.U32(InventoryVoxelEnvelope::Magic);
    Writer.U16(Schema);
    Writer.U8(InventoryVoxelEnvelope::Full);
    Writer.U8(0);
    Writer.Blob(Body, FInventorySnapshotCodec::MaximumBytes);
    Writer.U32(FCrc::MemCrc32(Body.GetData(), Body.Num()));
    FVoxelBlockEntityState Candidate;
    Candidate.Kind = Kind;
    Candidate.Schema = Schema;
    if(!Writer.Finish(Candidate.Payload))
    {
        OutError = TEXT("Inventory.VoxelEntityTooLarge");
        return false;
    }
    OutState = MoveTemp(Candidate);
    return true;
}

bool FInventoryVoxelEntityCodec::Decode(const FVoxelBlockEntityState& InState, const FInventoryItemRegistry& InRegistry,
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FInventorySnapshot& OutSnapshot, FString& OutError)
{
    check(IsInGameThread());
    uint8 Mode = 255;
    TConstArrayView<uint8> Body;
    if(!InventoryVoxelEnvelope::Read(InState, Mode, Body) || Mode != InventoryVoxelEnvelope::Full)
    {
        OutError = TEXT("Inventory.VoxelEntityNotInitialized");
        return false;
    }
    return FInventorySnapshotCodec::Decode(Body, InRegistry, InProfiles, OutSnapshot, OutError);
}
