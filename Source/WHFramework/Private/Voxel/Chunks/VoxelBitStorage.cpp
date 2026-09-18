#include "Voxel/Chunks/VoxelBitStorage.h"
void FVoxelBitStorage::Clear(){Bits=0;Count=0;Mask=0;Words.Empty();}
bool FVoxelBitStorage::Reset(uint8 B,int32 N)
{
    if(B<1||B>32||N<0||N>4096)return false;
    Bits=B;Count=N;Mask=(uint64(1)<<B)-1;
    Words.Empty();Words.SetNumZeroed(int32((uint64(B)*N+63)/64));return true;
}
uint32 FVoxelBitStorage::Get(int32 I)const
{
    check(I>=0&&I<Count);uint64 P=uint64(I)*Bits;int32 W=int32(P/64);uint32 S=uint32(P%64);
    uint64 V=Words[W]>>S;
    if(S+Bits>64)V|=Words[W+1]<<(64-S);
    return uint32(V&Mask);
}
void FVoxelBitStorage::Set(int32 I,uint32 V)
{
    check(I>=0&&I<Count&&uint64(V)<=Mask);uint64 P=uint64(I)*Bits;
    int32 W=int32(P/64);uint32 S=uint32(P%64);uint64 E=uint64(V)&Mask;
    Words[W]=(Words[W]&~(Mask<<S))|(E<<S);
    if(S+Bits>64)
    {
        uint32 N=S+Bits-64;uint64 M=(uint64(1)<<N)-1;
        Words[W+1]=(Words[W+1]&~M)|(E>>(64-S));
    }
}
bool FVoxelBitStorage::Import(uint8 B,int32 N,const TArray<uint64>& W)
{
    if(B<1||B>32||N<0||N>4096||W.Num()!=int32((uint64(B)*N+63)/64))return false;
    const uint32 Tail=uint32(uint64(B)*N%64);
    if(Tail&&W.Num()&&(W.Last()>>Tail)!=0)return false;
    FVoxelBitStorage T;if(!T.Reset(B,N))return false;T.Words=W;*this=MoveTemp(T);return true;
}
