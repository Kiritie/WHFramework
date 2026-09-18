#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
bool FVoxelWorldSaveAdapter::Capture(FVoxelWorldRuntime&R,const FVoxelWorldManifest&M,
    TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe>Registry,const FVoxelRegionStore&Store,FString&E)
{
    check(IsInGameThread());if(Active.IsSet()||!R.Authority()||!Registry){E=TEXT("Save capture busy or not authority");return false;}
    FVoxelWorldSaveCapture C;C.TransactionId=FGuid::NewGuid();C.WorldEpoch=R.Epoch();C.Manifest=M;C.Registry=Registry;C.SourceDirectory=Store.GetSourceDirectory();
    auto Keys=R.ResidentKeys();Keys.Sort();
    for(const auto&K:Keys)
    {
        auto*S=R.Find(K);if(S&&S->Status==EVoxelSectionStatus::DataReady&&S->IsSaveDirty())
        {if(S->PinCount==MAX_uint32){E=TEXT("Section pin overflow");return false;}C.Sections.Add(S->Overlay);}
    }
    for(const auto&O:C.Sections)++R.Find(O.Key)->PinCount;
    Active=MoveTemp(C);E.Reset();return true;
}
bool FVoxelWorldSaveAdapter::WriteCapture(const FVoxelWorldSaveCapture&C,const FString&Temp,FString&E)
{
    if(!C.Registry||!C.TransactionId.IsValid()){E=TEXT("Invalid save capture");return false;}
    FVoxelRegionWritePlan P;P.TransactionId=C.TransactionId;P.SourceDirectory=C.SourceDirectory;
    for(const auto&O:C.Sections)
    {
        TArray<uint8>B;if(!FVoxelDeltaCodec::Encode(C.Manifest,*C.Registry,O,B)||!FVoxelRegionStore::StageSection(P,O.Key,MoveTemp(B)))
        {E=TEXT("Cannot encode section delta");return false;}
    }
    return FVoxelRegionStore::WritePendingRegions(P,Temp,E);
}
void FVoxelWorldSaveAdapter::Complete(FVoxelWorldRuntime&R,FVoxelRegionStore&S,bool Success,const FString&Committed)
{
    check(IsInGameThread());if(!Active.IsSet())return;
    if(Active->WorldEpoch==R.Epoch())
    {
        for(const auto&O:Active->Sections)
        {
            if(Success)R.MarkCommitted(O.Key,O.Revision);
            if(auto*Section=R.Find(O.Key)){check(Section->PinCount>0);--Section->PinCount;}
        }
        if(Success)S.AdvanceSource(Committed);
    }
    Active.Reset();
}
