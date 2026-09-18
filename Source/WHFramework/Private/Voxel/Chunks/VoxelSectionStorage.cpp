#include "Voxel/Chunks/VoxelSectionStorage.h"
namespace
{
uint8 BitsFor(int32 N){uint8 B=1;while((uint32(1)<<B)<uint32(N))++B;return B;}
uint64 PackedCost(int32 N){return uint64(N)*4+((uint64(BitsFor(N))*4096+63)/64)*8;}
}
FVoxelBlockState FVoxelSectionStorage::Get(uint16 I)const
{
    check(I<4096);
    if(Mode==EVoxelSectionStorageMode::Uniform)return Uniform;
    if(Mode==EVoxelSectionStorageMode::Direct)return FVoxelBlockState::Unpack(Direct[I]);
    return Palette[Indices.Get(I)];
}
void FVoxelSectionStorage::Fill(FVoxelBlockState S)
{
    Mode=EVoxelSectionStorageMode::Uniform;Uniform=S;Palette.Empty();Direct.Empty();Indices.Clear();WritesSinceCompact=0;
}
void FVoxelSectionStorage::Repack(uint8 B)
{
    FVoxelBitStorage T;const bool OK=T.Reset(B,4096);check(OK);
    for(int32 I=0;I<4096;++I)T.Set(I,Indices.Get(I));Indices=MoveTemp(T);
}
void FVoxelSectionStorage::MakeDirect()
{
    TArray<uint32> D;CopyToDense(D);Direct=MoveTemp(D);
    Palette.Empty();Indices.Clear();Mode=EVoxelSectionStorageMode::Direct;
}
void FVoxelSectionStorage::Set(uint16 I,FVoxelBlockState S)
{
    check(I<4096);if(Get(I)==S)return;
    if(Mode==EVoxelSectionStorageMode::Uniform)
    {
        Palette={Uniform,S};const bool OK=Indices.Reset(1,4096);check(OK);Indices.Set(I,1);
        Mode=EVoxelSectionStorageMode::Palette;
    }
    else if(Mode==EVoxelSectionStorageMode::Direct)Direct[I]=S.Pack();
    else
    {
        int32 P=Palette.IndexOfByKey(S);
        if(P==INDEX_NONE)
        {
            const int32 N=Palette.Num()+1;
            if(PackedCost(N)>=4096*4)
            {MakeDirect();Direct[I]=S.Pack();}
            else
            {
                P=Palette.Add(S);uint8 B=BitsFor(Palette.Num());
                if(B!=Indices.GetBits())Repack(B);Indices.Set(I,uint32(P));
            }
        }
        else Indices.Set(I,uint32(P));
    }
    if(++WritesSinceCompact>=1024)Compact();
}
void FVoxelSectionStorage::CopyToDense(TArray<uint32>& O)const
{
    O.SetNumUninitialized(4096);for(uint16 I=0;I<4096;++I)O[I]=Get(I).Pack();
}
bool FVoxelSectionStorage::BuildFromDense(TConstArrayView<uint32> V)
{
    if(V.Num()!=4096)return false;
    TArray<FVoxelBlockState> P;TArray<uint16> Codes;Codes.SetNumUninitialized(4096);
    TMap<uint32,uint16> Lookup;
    for(int32 I=0;I<4096;++I)
    {
        const uint16* Existing=Lookup.Find(V[I]);
        if(Existing)Codes[I]=*Existing;
        else{uint16 C=uint16(P.Num());Lookup.Add(V[I],C);P.Add(FVoxelBlockState::Unpack(V[I]));Codes[I]=C;}
    }
    if(P.Num()==1){Fill(P[0]);return true;}
    FVoxelSectionStorage T;
    if(PackedCost(P.Num())>=16384)
    {T.Mode=EVoxelSectionStorageMode::Direct;T.Direct.Append(V.GetData(),V.Num());}
    else
    {
        T.Mode=EVoxelSectionStorageMode::Palette;T.Palette=MoveTemp(P);
        if(!T.Indices.Reset(BitsFor(T.Palette.Num()),4096))return false;
        for(int32 I=0;I<4096;++I)T.Indices.Set(I,Codes[I]);
    }
    *this=MoveTemp(T);return true;
}
void FVoxelSectionStorage::Compact()
{
    TArray<uint32>D;CopyToDense(D);const bool OK=BuildFromDense(D);check(OK);WritesSinceCompact=0;
}
uint64 FVoxelSectionStorage::PayloadBytes()const
{
    if(Mode==EVoxelSectionStorageMode::Uniform)return 4;
    if(Mode==EVoxelSectionStorageMode::Direct)return uint64(Direct.Num())*4;
    return uint64(Palette.Num())*4+uint64(Indices.GetWords().Num())*8;
}
