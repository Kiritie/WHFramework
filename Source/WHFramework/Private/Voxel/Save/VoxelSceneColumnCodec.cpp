#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "SaveGame/SaveDataSerializer.h"
FString FVoxelSceneColumnCodec::RelativePath(FIntPoint C)
{return FString::Printf(TEXT("voxel/actors/c_%d_%d.bin"),C.X,C.Y);}
bool FVoxelSceneColumnCodec::Encode(const TArray<FVoxelSavedSceneActor>&A,const FParameter& Project,TArray<uint8>&O,FString&E)
{
    check(IsInGameThread());if(A.Num()>1024){E=TEXT("Too many scene actors in one column");return false;}
    TArray<FVoxelSavedSceneActor>Sorted=A;Sorted.Sort([](const auto&X,const auto&Y){return X.Id.ToString()<Y.Id.ToString();});
    FVoxelByteWriter W(8*1024*1024);W.U32(0x33414356);W.U16(uint16(Sorted.Num()));TSet<FGuid>Seen;
    for(auto&X:Sorted)
    {
        TArray<uint8>B;if(!X.Id.IsValid()||Seen.Contains(X.Id)||!X.Data.HasValue()||
            !FSaveDataSerializer::MarkSaveDataSaved(X.Data)||!FSaveDataSerializer::SerializeParameter(X.Data,B)||B.Num()>256*1024)
        {E=TEXT("Invalid scene actor save payload");return false;}Seen.Add(X.Id);W.Guid(X.Id);W.Blob(B,256*1024);
    }
    TArray<uint8> Meta;
    if(Project.HasValue()){FParameter Copy=Project;if(!FSaveDataSerializer::MarkSaveDataSaved(Copy)||!FSaveDataSerializer::SerializeParameter(Copy,Meta)||Meta.Num()>1024*1024){E=TEXT("Invalid scene project metadata");return false;}}
    W.Blob(Meta,1024*1024);
    if(!W.Finish(O)){E=TEXT("Scene column save exceeds 8MiB");return false;}E.Reset();return true;
}
bool FVoxelSceneColumnCodec::Decode(TConstArrayView<uint8>B,TArray<FVoxelSavedSceneActor>&O,FParameter& Project,FString&E)
{
    check(IsInGameThread());if(B.Num()>8*1024*1024)return false;FVoxelByteReader R(B);if(R.U32()!=0x33414356)return false;
    uint16 N=R.U16();if(N>1024)return false;TArray<FVoxelSavedSceneActor>T;TSet<FGuid>Seen;
    for(uint16 I=0;I<N;++I){FVoxelSavedSceneActor X;X.Id=R.Guid();auto Raw=R.Blob(256*1024);
        if(!R.IsValid()||!X.Id.IsValid()||Seen.Contains(X.Id)||!FSaveDataSerializer::DeserializeParameter(Raw,X.Data))
        {E=TEXT("Invalid scene actor save record");return false;}Seen.Add(X.Id);T.Add(MoveTemp(X));}
    auto Meta=R.Blob(1024*1024);FParameter Data;if(!Meta.IsEmpty()&&!FSaveDataSerializer::DeserializeParameter(Meta,Data))return false;
    if(!R.End())return false;Project=MoveTemp(Data);O=MoveTemp(T);E.Reset();return true;
}
