#include "Voxel/Save/VoxelBlockEntityCodec.h"

#include "Misc/ScopeRWLock.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace VoxelEntityRegistry
{
    struct FEntry
    {
        FVoxelBlockEntityCodec::FMakeDefault MakeDefault = nullptr;
        FVoxelBlockEntityCodec::FValidate Validate = nullptr;
    };

    FRWLock Lock;
    TMap<uint16, FEntry> Entries;

    FEntry Find(uint16 InKind)
    {
        FReadScopeLock Scope(Lock);
        return Entries.FindRef(InKind);
    }
}

bool FVoxelBlockEntityCodec::Register(uint16 InKind, FMakeDefault InMakeDefault, FValidate InValidate)
{
    if(InKind <= 2 || !InMakeDefault || !InValidate)
    {
        return false;
    }
    FWriteScopeLock Scope(VoxelEntityRegistry::Lock);
    if(const VoxelEntityRegistry::FEntry* Existing = VoxelEntityRegistry::Entries.Find(InKind))
    {
        return Existing->MakeDefault == InMakeDefault && Existing->Validate == InValidate;
    }
    VoxelEntityRegistry::Entries.Add(InKind, {InMakeDefault, InValidate});
    return true;
}

bool FVoxelBlockEntityCodec::IsRegistered(uint16 InKind)
{
    return InKind == 2 || VoxelEntityRegistry::Find(InKind).Validate != nullptr;
}

bool FVoxelBlockEntityCodec::MakeDefault(uint16 InKind, FVoxelBlockEntityState& OutState, uint8 InVariant)
{
    FVoxelBlockEntityState Candidate;
    Candidate.Kind = InKind;
    if(InKind == 2)
    {
        if(InVariant != 0)
        {
            return false;
        }
        FVoxelByteWriter Writer(8);
        Writer.U64(0);
        if(!Writer.Finish(Candidate.Payload))
        {
            return false;
        }
    }
    else
    {
        const VoxelEntityRegistry::FEntry Entry = VoxelEntityRegistry::Find(InKind);
        if(!Entry.MakeDefault || !Entry.MakeDefault(Candidate, InVariant) || Candidate.Kind != InKind)
        {
            return false;
        }
    }
    if(Candidate.Payload.Num() > MaximumDefaultPayloadBytes || !Validate(Candidate))
    {
        return false;
    }
    OutState = MoveTemp(Candidate);
    return true;
}

bool FVoxelBlockEntityCodec::Validate(const FVoxelBlockEntityState& InState)
{
    if(InState.Schema == 0 || InState.Payload.Num() > FVoxelDeltaCodec::MaximumEntityPayloadBytes)
    {
        return false;
    }
    if(InState.Kind == 2)
    {
        if(InState.Schema != 1)
        {
            return false;
        }
        FVoxelByteReader Reader(InState.Payload);
        Reader.U64();
        return Reader.End();
    }
    const VoxelEntityRegistry::FEntry Entry = VoxelEntityRegistry::Find(InState.Kind);
    return Entry.Validate && Entry.Validate(InState);
}

bool FVoxelBlockEntityCodec::IncrementCounter(const FVoxelBlockEntityState& InState, FVoxelBlockEntityState& OutState)
{
    if(InState.Kind != 2 || !Validate(InState))
    {
        return false;
    }
    FVoxelByteReader Reader(InState.Payload);
    const uint64 Value = Reader.U64();
    if(Value == MAX_uint64)
    {
        return false;
    }
    FVoxelByteWriter Writer(8);
    Writer.U64(Value + 1);
    FVoxelBlockEntityState Candidate;
    Candidate.Kind = 2;
    if(!Writer.Finish(Candidate.Payload))
    {
        return false;
    }
    OutState = MoveTemp(Candidate);
    return true;
}
