#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Misc/ScopeRWLock.h"

namespace
{
    struct FVoxelEntityCodecEntry
    {
        FVoxelBlockEntityCodec::FMakeDefault MakeDefault = nullptr;
        FVoxelBlockEntityCodec::FValidate Validate = nullptr;
    };

    FRWLock VoxelEntityCodecLock;
    TMap<uint16, FVoxelEntityCodecEntry> VoxelEntityCodecs;

    FVoxelEntityCodecEntry FindVoxelEntityCodec(uint16 Kind)
    {
        FReadScopeLock Lock(VoxelEntityCodecLock);
        return VoxelEntityCodecs.FindRef(Kind);
    }
}

bool FVoxelBlockEntityCodec::Register(uint16 Kind, FMakeDefault Make, FValidate Check)
{
    if (Kind <= 2 || !Make || !Check)
    {
        return false;
    }
    FWriteScopeLock Lock(VoxelEntityCodecLock);
    if (const FVoxelEntityCodecEntry* Entry = VoxelEntityCodecs.Find(Kind))
    {
        return Entry->MakeDefault == Make && Entry->Validate == Check;
    }
    VoxelEntityCodecs.Add(Kind, {Make, Check});
    return true;
}

bool FVoxelBlockEntityCodec::IsRegistered(uint16 Kind)
{
    return Kind == 1 || Kind == 2 || FindVoxelEntityCodec(Kind).Validate != nullptr;
}
bool FVoxelBlockEntityCodec::EncodeContainer(const TArray<FVoxelItemStack>&S,FVoxelBlockEntityState&O)
{
    if(S.Num()!=27)return false;FVoxelByteWriter W(32768);W.U8(27);
    for(const auto&I:S)
    {
        if(I.Count<0||I.Count>100000||I.Level<0||I.Level>10000||((I.Count>0)!=I.ID.IsValid())||(I.Count==0&&I.Level!=0))return false;
        W.String(I.ID.IsValid()?I.ID.ToString():FString(),256);W.I32(I.Count);W.I32(I.Level);
    }
    FVoxelBlockEntityState T;T.Kind=1;if(!W.Finish(T.Payload))return false;O=MoveTemp(T);return true;
}
bool FVoxelBlockEntityCodec::DecodeContainer(const FVoxelBlockEntityState&S,TArray<FVoxelItemStack>&O)
{
    if(S.Kind!=1||S.Schema!=1||S.Payload.Num()>32768)return false;FVoxelByteReader R(S.Payload);if(R.U8()!=27)return false;
    TArray<FVoxelItemStack>T;for(int32 J=0;J<27;++J)
    {
        FString Name=R.String(256);FVoxelItemStack I;I.ID=Name.IsEmpty()?FPrimaryAssetId():FPrimaryAssetId(Name);
        I.Count=R.I32();I.Level=R.I32();
        if(!R.IsValid()||I.Count<0||I.Count>100000||I.Level<0||I.Level>10000||((I.Count>0)!=I.ID.IsValid())||
            (I.Count==0&&(!Name.IsEmpty()||I.Level!=0)))return false;T.Add(I);
    }
    if(!R.End())return false;O=MoveTemp(T);return true;
}
bool FVoxelBlockEntityCodec::MakeDefault(uint16 K,FVoxelBlockEntityState&O,uint8 Variant)
{
    if ((K == 1 || K == 2) && Variant != 0)return false;
    if(K==1){TArray<FVoxelItemStack>S;S.SetNum(27);return EncodeContainer(S,O);}
    FVoxelBlockEntityState T;T.Kind=K;FVoxelByteWriter W(1024);
    if(K==2)W.U64(0);
    else
    {
        const FVoxelEntityCodecEntry Entry = FindVoxelEntityCodec(K);
        if (!Entry.MakeDefault || !Entry.MakeDefault(T, Variant) || T.Kind != K || !Entry.Validate(T))
        {
            return false;
        }
        O = MoveTemp(T);
        return true;
    }
    if(!W.Finish(T.Payload))return false;O=MoveTemp(T);return true;
}
bool FVoxelBlockEntityCodec::Validate(const FVoxelBlockEntityState&S)
{
    if(S.Schema==0||S.Payload.Num()>32768)return false;
    if(S.Kind==1){TArray<FVoxelItemStack>T;return DecodeContainer(S,T);}
    FVoxelByteReader R(S.Payload);
    if(S.Kind==2){if(S.Schema!=1)return false;R.U64();}
    else
    {
        const FVoxelEntityCodecEntry Entry = FindVoxelEntityCodec(S.Kind);
        return Entry.Validate && Entry.Validate(S);
    }
    return R.End();
}
bool FVoxelBlockEntityCodec::IncrementCounter(const FVoxelBlockEntityState&I,FVoxelBlockEntityState&O)
{
    if(I.Kind!=2||!Validate(I))return false;FVoxelByteReader R(I.Payload);uint64 V=R.U64();if(V==MAX_uint64)return false;
    FVoxelByteWriter W(8);W.U64(V+1);FVoxelBlockEntityState T;T.Kind=2;if(!W.Finish(T.Payload))return false;O=MoveTemp(T);return true;
}
