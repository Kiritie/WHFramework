#include "Voxel/Chunks/VoxelSectionCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
bool FVoxelSectionCodec::Encode(const FVoxelSectionStorage&S,TArray<uint8>&O)
{
    TArray<uint32>D;S.CopyToDense(D);TArray<uint32>P;TMap<uint32,uint16>M;TArray<uint16>C;
    C.SetNumUninitialized(4096);
    for(int32 I=0;I<4096;++I)
    {
        const uint16* V=M.Find(D[I]);
        if(V)C[I]=*V;else{uint16 J=uint16(P.Num());M.Add(D[I],J);P.Add(D[I]);C[I]=J;}
    }
    uint8 B=1;while((uint32(1)<<B)<uint32(P.Num()))++B;
    bool Direct=uint64(P.Num())*4+uint64(B)*512>=16384;
    FVoxelByteWriter W(20000);W.U32(0x31535856);W.U8(P.Num()==1?0:Direct?2:1);
    if(P.Num()==1)W.U32(P[0]);
    else if(Direct){for(uint32 V:D)W.U32(V);}
    else
    {
        W.U16(uint16(P.Num()));for(uint32 V:P)W.U32(V);
        FVoxelBitStorage Packed;if(!Packed.Reset(B,4096))return false;
        for(int32 I=0;I<4096;++I)Packed.Set(I,C[I]);
        W.U8(B);W.U16(uint16(Packed.GetWords().Num()));for(uint64 V:Packed.GetWords())W.U64(V);
    }
    return W.Finish(O);
}
bool FVoxelSectionCodec::Decode(TConstArrayView<uint8>B,TFunctionRef<bool(FVoxelBlockState)>Valid,FVoxelSectionStorage&O)
{
    if(B.Num()>20000)return false;FVoxelByteReader R(B);if(R.U32()!=0x31535856)return false;
    uint8 Mode=R.U8();TArray<uint32>D;D.SetNumUninitialized(4096);
    if(Mode==0){uint32 V=R.U32();if(!Valid(FVoxelBlockState::Unpack(V)))return false;for(auto&X:D)X=V;}
    else if(Mode==2)
    {for(auto&V:D){V=R.U32();if(!R.IsValid()||!Valid(FVoxelBlockState::Unpack(V)))return false;}}
    else if(Mode==1)
    {
        int32 N=R.U16();if(N<2||N>4096)return false;TArray<uint32>P;TSet<uint32>Seen;
        for(int32 I=0;I<N;++I){uint32 V=R.U32();if(!R.IsValid()||Seen.Contains(V)||!Valid(FVoxelBlockState::Unpack(V)))return false;Seen.Add(V);P.Add(V);}
        uint8 Bits=R.U8(),Expected=1;while((uint32(1)<<Expected)<uint32(N))++Expected;
        int32 WN=R.U16();if(Bits!=Expected||WN!=(int32(Bits)*4096+63)/64)return false;
        TArray<uint64>Words;for(int32 I=0;I<WN;++I)Words.Add(R.U64());if(!R.IsValid())return false;
        FVoxelBitStorage V;if(!V.Import(Bits,4096,Words))return false;
        for(int32 I=0;I<4096;++I){uint32 J=V.Get(I);if(J>=uint32(N))return false;D[I]=P[J];}
    }
    else return false;
    if(!R.End())return false;FVoxelSectionStorage T;if(!T.BuildFromDense(D))return false;O=MoveTemp(T);return true;
}
uint64 FVoxelSectionCodec::Fingerprint(const FVoxelSectionStorage&S)
{
    FVoxelByteWriter W(16384);for(uint16 I=0;I<4096;++I)W.U32(S.Get(I).Pack());TArray<uint8>B;
    const bool OK=W.Finish(B);check(OK);return VoxelBinary::Hash(B);
}
