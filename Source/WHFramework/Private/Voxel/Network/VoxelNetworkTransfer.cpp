#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Misc/Crc.h"
bool FVoxelNetworkTransfer::Enqueue(TArray<uint8>&&M)
{
    if(M.IsEmpty()||M.Num()>2*1024*1024||Pending.Num()>=64||PendingBytes+M.Num()>8*1024*1024||NextID==MAX_uint64)return false;
    FSend S;S.ID=NextID++;S.CRC=FCrc::MemCrc32(M.GetData(),M.Num());S.Bytes=MoveTemp(M);PendingBytes+=S.Bytes.Num();Pending.Add(MoveTemp(S));return true;
}
void FVoxelNetworkTransfer::Tick(double Now,TFunctionRef<void(const TArray<uint8>&)>Send)
{
    if(LastTick==0)LastTick=Now;Tokens=FMath::Min(32768.0,Tokens+FMath::Clamp(Now-LastTick,0.0,1.0)*131072);LastTick=Now;
    for(int32 Count=0;Count<4&&!Pending.IsEmpty();++Count)
    {
        auto&S=Pending[0];int32 N=FMath::Min(4096,S.Bytes.Num()-S.Offset);if(Tokens<N+32)break;
        FVoxelByteWriter W(4608);W.U32(0x32475246);W.U64(S.ID);W.U16(uint16(S.Offset/4096));
        W.U16(uint16((S.Bytes.Num()+4095)/4096));W.U32(uint32(S.Bytes.Num()));W.U32(S.CRC);
        W.Blob(MakeArrayView(S.Bytes).Slice(S.Offset,N),4096);TArray<uint8>B;if(!W.Finish(B))break;
        Send(B);Tokens-=B.Num();S.Offset+=N;
        if(S.Offset==S.Bytes.Num()){PendingBytes-=S.Bytes.Num();Pending.RemoveAt(0,1,EAllowShrinking::No);}
    }
    for(auto It=Receiving.CreateIterator();It;++It)if(Now-It.Value().Since>15){ReceivingBytes-=It.Value().Total;It.RemoveCurrent();}
}
bool FVoxelNetworkTransfer::Receive(TConstArrayView<uint8>P,double Now,TArray<uint8>&Completed)
{
    Completed.Reset();if(P.Num()>4608)return false;FVoxelByteReader R(P);if(R.U32()!=0x32475246)return false;
    uint64 ID=R.U64();uint16 Index=R.U16(),Num=R.U16();uint32 Total=R.U32(),CRC=R.U32();auto Data=R.Blob(4096);
    if(!R.End()||!ID||!Total||Total>2*1024*1024||Num!=(Total+4095)/4096||Index>=Num||
        Data.Num()!=FMath::Min<uint32>(4096,Total-uint32(Index)*4096))return false;
    auto*S=Receiving.Find(ID);
    if(!S)
    {
        if(Receiving.Num()>=4||ReceivingBytes+Total>8*1024*1024)return false;
        FReceive N;N.Total=int32(Total);N.CRC=CRC;N.Bytes.SetNumUninitialized(N.Total);N.Got.Init(false,Num);N.Since=Now;
        ReceivingBytes+=N.Total;S=&Receiving.Add(ID,MoveTemp(N));
    }
    if(S->Total!=int32(Total)||S->CRC!=CRC||S->Got.Num()!=Num)return false;
    int32 Offset=int32(Index)*4096;
    if(S->Got[Index])return FMemory::Memcmp(S->Bytes.GetData()+Offset,Data.GetData(),Data.Num())==0;
    FMemory::Memcpy(S->Bytes.GetData()+Offset,Data.GetData(),Data.Num());S->Got[Index]=true;++S->Count;
    if(S->Count==Num)
    {
        bool OK=FCrc::MemCrc32(S->Bytes.GetData(),S->Bytes.Num())==S->CRC;ReceivingBytes-=S->Total;
        if(OK)Completed=MoveTemp(S->Bytes);Receiving.Remove(ID);return OK;
    }
    return true;
}
void FVoxelNetworkTransfer::Reset(){Pending.Reset();Receiving.Reset();PendingBytes=0;ReceivingBytes=0;LastTick=0;Tokens=32768;}
