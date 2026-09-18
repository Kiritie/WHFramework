#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Misc/Compression.h"
#include "Misc/Crc.h"
#include "Containers/StringConv.h"
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
    TArray<uint8>T;if(N)T.Append(Data.GetData()+Pos,N);Pos+=N;O=MoveTemp(T);return true;
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
bool VoxelBinary::Compress(TConstArrayView<uint8>R,TArray<uint8>&O,int32 Limit)
{
    if(R.Num()<=0||R.Num()>Limit)return false;
    int32 N=FCompression::CompressMemoryBound(NAME_Zlib,R.Num());
    if(N<=0||N>Limit+65536)return false;
    TArray<uint8>C;C.SetNumUninitialized(N);
    if(!FCompression::CompressMemory(NAME_Zlib,C.GetData(),N,R.GetData(),R.Num()))return false;
    C.SetNum(N);FVoxelByteWriter W(Limit+65536+16);
    W.U32(0x315a5856);W.U32(uint32(R.Num()));W.U32(FCrc::MemCrc32(R.GetData(),R.Num()));
    W.Blob(C,Limit+65536);return W.Finish(O);
}
bool VoxelBinary::Decompress(TConstArrayView<uint8>S,TArray<uint8>&O,int32 Limit)
{
    if(S.Num()>Limit+65536+16)return false;
    FVoxelByteReader R(S);uint32 HeaderMagic=R.U32(),N=R.U32(),CRC=R.U32();auto C=R.Blob(Limit+65536);
    if(!R.End()||HeaderMagic!=0x315a5856||N==0||N>uint32(Limit)||C.IsEmpty())return false;
    TArray<uint8>T;T.SetNumUninitialized(int32(N));
    if(!FCompression::UncompressMemory(NAME_Zlib,T.GetData(),T.Num(),C.GetData(),C.Num())||
        FCrc::MemCrc32(T.GetData(),T.Num())!=CRC)return false;
    O=MoveTemp(T);return true;
}
