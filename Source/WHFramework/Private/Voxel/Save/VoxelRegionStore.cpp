#include "Voxel/Save/VoxelRegionStore.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
namespace
{
constexpr uint32 RegionMagic=0x32525856;
constexpr uint32 MaxRecord=1024*1024+65536+16;
struct FEntry{uint16 Local=0;uint64 Offset=0;uint32 Size=0,Crc=0;};
struct FRegionIndex{TArray<FEntry>Entries;};
FVoxelSectionKey Region(const FVoxelSectionKey&K){return {VoxelCoord::FloorDiv(K.X,8),VoxelCoord::FloorDiv(K.Y,8),VoxelCoord::FloorDiv(K.Z,8)};}
uint16 LocalKey(const FVoxelSectionKey&K){auto R=Region(K);return uint16(K.X-R.X*8+(K.Y-R.Y*8)*8+(K.Z-R.Z*8)*64);}
FString Path(const FString&Dir,const FVoxelSectionKey&R){return FPaths::Combine(Dir,TEXT("voxel"),TEXT("regions"),FString::Printf(TEXT("r_%d_%d_%d.bin"),R.X,R.Y,R.Z));}
bool ReadBytes(FArchive&Ar,int64 Offset,int32 Size,TArray<uint8>&Out)
{
    if(Offset<0||Size<0||Offset>Ar.TotalSize()||int64(Size)>Ar.TotalSize()-Offset)return false;
    TArray<uint8>B;B.SetNumUninitialized(Size);Ar.Seek(Offset);if(Size)Ar.Serialize(B.GetData(),Size);
    if(Ar.IsError())return false;Out=MoveTemp(B);return true;
}
EVoxelRegionRead ReadIndex(const FString&P,FRegionIndex&Out,FString&E)
{
    if(!IFileManager::Get().FileExists(*P))return EVoxelRegionRead::Missing;
    TUniquePtr<FArchive>F(IFileManager::Get().CreateFileReader(*P));if(!F){E=TEXT("Cannot open region file");return EVoxelRegionRead::Failed;}
    TArray<uint8>Head;if(!ReadBytes(*F,0,8,Head)){E=TEXT("Truncated region header");return EVoxelRegionRead::Failed;}
    FVoxelByteReader H(Head);uint32 M=H.U32();uint16 V=H.U16(),N=H.U16();
    if(M!=RegionMagic||V!=2||N>512){E=TEXT("Region schema mismatch");return EVoxelRegionRead::Failed;}
    TArray<uint8>Table;if(!ReadBytes(*F,8,int32(N)*18,Table)){E=TEXT("Truncated region index");return EVoxelRegionRead::Failed;}
    FVoxelByteReader Q(Table);FRegionIndex T;uint64 Expected=8+uint64(N)*18;int32 Prev=-1;
    for(uint16 I=0;I<N;++I)
    {
        FEntry A;A.Local=Q.U16();A.Offset=Q.U64();A.Size=Q.U32();A.Crc=Q.U32();
        if(A.Local>=512||A.Local<=Prev||A.Offset!=Expected||A.Size==0||A.Size>MaxRecord||A.Offset>uint64(F->TotalSize())||A.Size>uint64(F->TotalSize())-A.Offset)
        {E=TEXT("Invalid region index bounds");return EVoxelRegionRead::Failed;}
        Prev=A.Local;Expected+=A.Size;T.Entries.Add(A);
    }
    if(!Q.End()||Expected!=uint64(F->TotalSize())){E=TEXT("Region trailing or missing bytes");return EVoxelRegionRead::Failed;}
    Out=MoveTemp(T);return EVoxelRegionRead::Loaded;
}
bool ReadEntry(FArchive&F,const FEntry&E,TArray<uint8>&Out)
{
    TArray<uint8>B;if(!ReadBytes(F,int64(E.Offset),int32(E.Size),B)||FCrc::MemCrc32(B.GetData(),B.Num())!=E.Crc)return false;Out=MoveTemp(B);return true;
}
}
void FVoxelRegionStore::SetSource(const FGuid&Id,int32 G,FSaveGameStorage*S)
{check(IsInGameThread());SourceDirectory=S&&Id.IsValid()&&G>0?S->GetGenerationDir(Id,G):FString();}
FVoxelRegionReadView FVoxelRegionStore::CaptureRead(const FVoxelSectionKey&K)const
{return {SourceDirectory,K};}
EVoxelRegionRead FVoxelRegionStore::Read(const FVoxelRegionReadView&V,TArray<uint8>&O,FString&E)
{
    if(V.SourceDirectory.IsEmpty())return EVoxelRegionRead::Missing;
    FString P=Path(V.SourceDirectory,Region(V.Key));FRegionIndex I;auto R=ReadIndex(P,I,E);if(R!=EVoxelRegionRead::Loaded)return R;
    const uint16 L=LocalKey(V.Key);const FEntry*Found=I.Entries.FindByPredicate([L](const FEntry&A){return A.Local==L;});
    if(!Found)return EVoxelRegionRead::Missing;TUniquePtr<FArchive>F(IFileManager::Get().CreateFileReader(*P));
    if(!F||!ReadEntry(*F,*Found,O)){E=TEXT("Region record read/checksum failed");return EVoxelRegionRead::Failed;}return EVoxelRegionRead::Loaded;
}
bool FVoxelRegionStore::ReadRange(const FString&Directory,const FVoxelSectionKey&Min,const FVoxelSectionKey&Max,const TSet<FVoxelSectionKey>&SupersededResident,TMap<FVoxelSectionKey,TArray<uint8>>&Out,bool&bOverBudget,FString&Error,const std::atomic_bool*Cancel)
{
    bOverBudget=false;Error.Reset();const int64 DX=int64(Max.X)-Min.X,DY=int64(Max.Y)-Min.Y,DZ=int64(Max.Z)-Min.Z;
    if(DX<=0||DY<=0||DZ<=0||DX>16||DY>16||DZ>16){Error=TEXT("Proxy range must fit at most 16x16x16 sections");return false;}
    TMap<FVoxelSectionKey,TArray<uint8>> Result;if(Directory.IsEmpty()){Out=MoveTemp(Result);return true;}
    if(!IFileManager::Get().DirectoryExists(*Directory)){Error=TEXT("Captured save generation no longer exists");return false;}
    const FVoxelSectionKey First=Region(Min),Last=Region({Max.X-1,Max.Y-1,Max.Z-1});uint64 Bytes=0;constexpr uint64 MaxBytes=32ull*1024*1024;
    for(int32 Z=First.Z;Z<=Last.Z;++Z)for(int32 Y=First.Y;Y<=Last.Y;++Y)for(int32 X=First.X;X<=Last.X;++X)
    {
        if(Cancel&&Cancel->load(std::memory_order_relaxed)){Error=TEXT("Canceled proxy read");return false;}
        const FVoxelSectionKey RK{X,Y,Z};const FString Filename=Path(Directory,RK);FRegionIndex Index;const auto Status=ReadIndex(Filename,Index,Error);if(Status==EVoxelRegionRead::Failed)return false;if(Status==EVoxelRegionRead::Missing)continue;
        TUniquePtr<FArchive>File(IFileManager::Get().CreateFileReader(*Filename));if(!File){Error=TEXT("Cannot open indexed proxy region");return false;}
        for(const FEntry&Entry:Index.Entries){const FVoxelSectionKey Key{X*8+Entry.Local%8,Y*8+(Entry.Local/8)%8,Z*8+Entry.Local/64};if(Key.X<Min.X||Key.X>=Max.X||Key.Y<Min.Y||Key.Y>=Max.Y||Key.Z<Min.Z||Key.Z>=Max.Z||SupersededResident.Contains(Key))continue;Bytes+=Entry.Size;if(Bytes>MaxBytes){bOverBudget=true;Out.Reset();return true;}TArray<uint8>Data;if(!ReadEntry(*File,Entry,Data)){Error=TEXT("Proxy region payload CRC/read failure");return false;}Result.Add(Key,MoveTemp(Data));}
    }
    if(!IFileManager::Get().DirectoryExists(*Directory)){Error=TEXT("Save generation changed while reading");return false;}Out=MoveTemp(Result);return true;
}
bool FVoxelRegionStore::StageSection(FVoxelRegionWritePlan&P,const FVoxelSectionKey&K,TArray<uint8>&&B)
{
    if(B.IsEmpty()||B.Num()>int32(MaxRecord))return false;FVoxelRegionOperation O;
    O.Bytes=MakeShared<TArray<uint8>,ESPMode::ThreadSafe>(MoveTemp(B));P.Operations.Add(K,MoveTemp(O));return true;
}
void FVoxelRegionStore::StageDelete(FVoxelRegionWritePlan&P,const FVoxelSectionKey&K)
{FVoxelRegionOperation O;O.bDelete=true;P.Operations.Add(K,MoveTemp(O));}
bool FVoxelRegionStore::WritePendingRegions(const FVoxelRegionWritePlan&P,const FString&Temp,FString&E)
{
    if(!P.TransactionId.IsValid()||Temp.IsEmpty()||(!P.SourceDirectory.IsEmpty()&&FPaths::IsSamePath(P.SourceDirectory,Temp)))
    {E=TEXT("Invalid region write target");return false;}
    TMap<FVoxelSectionKey,TArray<FVoxelSectionKey>>Groups;
    for(const auto&X:P.Operations)Groups.FindOrAdd(Region(X.Key)).Add(X.Key);
    for(const auto&G:Groups)
    {
        FString Source=Path(P.SourceDirectory,G.Key),Dest=Path(Temp,G.Key);FRegionIndex Old;
        if(!P.SourceDirectory.IsEmpty()&&ReadIndex(Source,Old,E)==EVoxelRegionRead::Failed)return false;
        struct FOutput{FEntry Entry;TSharedPtr<const TArray<uint8>,ESPMode::ThreadSafe>Replacement;};
        TMap<uint16,FOutput>Output;
        for(const auto&A:Old.Entries){FOutput O;O.Entry=A;Output.Add(A.Local,MoveTemp(O));}
        for(const auto&K:G.Value)
        {
            const auto&Op=P.Operations.FindChecked(K);uint16 L=LocalKey(K);
            if(Op.bDelete){Output.Remove(L);continue;}
            if(!Op.Bytes||Op.Bytes->IsEmpty()||Op.Bytes->Num()>int32(MaxRecord)){E=TEXT("Invalid staged region bytes");return false;}
            FOutput O;O.Entry.Local=L;O.Entry.Size=uint32(Op.Bytes->Num());O.Entry.Crc=FCrc::MemCrc32(Op.Bytes->GetData(),Op.Bytes->Num());O.Replacement=Op.Bytes;Output.Add(L,MoveTemp(O));
        }
        if(Output.IsEmpty())
        {
            if(IFileManager::Get().FileExists(*Dest)&&!IFileManager::Get().Delete(*Dest,false,true)){E=TEXT("Cannot remove empty target region");return false;}continue;
        }
        if(!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true)){E=TEXT("Cannot create region directory");return false;}
        TArray<uint16>Keys;Output.GetKeys(Keys);Keys.Sort();FVoxelByteWriter W(8+512*18);W.U32(RegionMagic);W.U16(2);W.U16(uint16(Keys.Num()));
        uint64 Offset=8+uint64(Keys.Num())*18;
        for(uint16 K:Keys){const auto&O=Output.FindChecked(K);W.U16(K);W.U64(Offset);W.U32(O.Entry.Size);W.U32(O.Entry.Crc);Offset+=O.Entry.Size;}
        TArray<uint8>Header;if(!W.Finish(Header))return false;TUniquePtr<FArchive>Writer(IFileManager::Get().CreateFileWriter(*Dest));
        if(!Writer){E=TEXT("Cannot write region");return false;}Writer->Serialize(Header.GetData(),Header.Num());
        TUniquePtr<FArchive>Reader;
        for(uint16 K:Keys)
        {
            const auto&O=Output.FindChecked(K);TArray<uint8>Copied;const TArray<uint8>*Bytes=O.Replacement.Get();
            if(!Bytes)
            {
                if(!Reader)Reader.Reset(IFileManager::Get().CreateFileReader(*Source));
                if(!Reader||!ReadEntry(*Reader,O.Entry,Copied)){E=TEXT("Cannot copy verified source record");return false;}Bytes=&Copied;
            }
            Writer->Serialize(const_cast<uint8*>(Bytes->GetData()),Bytes->Num());if(Writer->IsError()){E=TEXT("Region write failed");return false;}
        }
        if(!Writer->Close()){E=TEXT("Region close failed");return false;}
    }
    return true;
}
