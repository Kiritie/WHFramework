#include "Voxel/Rendering/VoxelProxyData.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
namespace
{
void WriteKey(FVoxelByteWriter& W,VoxelView::Key K){W.U8(K.level);W.I32(K.coordinate.x);W.I32(K.coordinate.y);W.I32(K.coordinate.z);}
VoxelView::Key ReadKey(FVoxelByteReader& R){VoxelView::Key K;K.level=R.U8();K.coordinate.x=R.I32();K.coordinate.y=R.I32();K.coordinate.z=R.I32();return K;}
}
bool FVoxelProxyData::ValidKey(VoxelView::Key K,bool Fine)
{
    if(K.level>4||(!Fine&&!K.level))return false;
    const int64 S=int64(16)<<K.level;
    for(int A=0;A<3;++A)
    {
        const int64 Lo=int64(K.coordinate[A])*S,Hi=Lo+S;
        const int64 Limit=A==2?32768:VoxelGen::CoordinateLimit;
        if(Lo< -Limit||Hi>Limit)return false;
    }
    return true;
}
bool FVoxelProxyData::Capture(VoxelView::Key K,uint64 Serial,uint64 Version,uint64 Fence,const FVoxelWorldManifest& M,
    TSharedPtr<const FVoxelGenerationPipeline,ESPMode::ThreadSafe> G,TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe> R,
    const FVoxelRegionStore& Store,const FVoxelWorldRuntime& World,FVoxelProxyRead& Out)
{
    check(IsInGameThread());if(!ValidKey(K)||!Serial||!G||!R)return false;
    FVoxelProxyRead T;T.Key=K;T.Serial=Serial;T.Version=Version;T.SaveFence=Fence;T.Manifest=M;T.Generator=G;T.Registry=R;T.SourceDirectory=Store.GetSourceDirectory();
    const auto B=K.Bounds();
    uint64 CellCount=0;
    for(const auto& S:World.ResidentKeys())
    {
        VoxelGen::I3 P{S.X*16,S.Y*16,S.Z*16};if(!B.Contains(P))continue;
        const auto* V=World.Find(S);
        if(V&&V->Status==EVoxelSectionStatus::DataReady)
        {
            CellCount+=V->Overlay.Blocks.Num();
            if(CellCount>131072){T.bRequiresSplit=true;T.Resident.Reset();break;}
            FVoxelSectionOverlay O;O.Key=S;O.Revision=V->Overlay.Revision;O.Blocks=V->Overlay.Blocks;T.Resident.Add(S,MoveTemp(O));
        }
    }
    Out=MoveTemp(T);return true;
}
bool FVoxelProxyData::Build(const FVoxelProxyRead& R,FVoxelProxyReply& Out,FString& E,const std::atomic_bool* Cancel)
{
    if(!R.Generator||!R.Registry||!ValidKey(R.Key))return false;
    FVoxelProxyReply Reply;Reply.Key=R.Key;Reply.Serial=R.Serial;Reply.Version=R.Version;
    if(R.bRequiresSplit){Reply.Mode=EVoxelProxyMode::Split;Out=MoveTemp(Reply);return true;}
    TMap<FVoxelSectionKey,TArray<uint8>> Disk;bool OverBudget=false;
    auto B=R.Key.Bounds();FVoxelSectionKey Lo{B.min.x/16,B.min.y/16,B.min.z/16},Hi{B.max.x/16,B.max.y/16,B.max.z/16};
    // Section-aligned min/max are exact, including negative multiples of 16.
    TSet<FVoxelSectionKey> ResidentKeys;for(const auto& X:R.Resident)ResidentKeys.Add(X.Key);
    if(!FVoxelRegionStore::ReadRange(R.SourceDirectory,Lo,Hi,ResidentKeys,Disk,OverBudget,E,Cancel))return false;
    if(OverBudget){Reply.Mode=EVoxelProxyMode::Split;Out=MoveTemp(Reply);return true;}
    std::vector<VoxelView::Override> Edits;
    auto Collect=[&](const FVoxelSectionOverlay& O)->bool
    {
        for(const auto& X:O.Blocks)
        {
            if(X.Key>=4096||!R.Registry->IsValid(X.Value)){E=TEXT("Invalid proxy overlay cell");return false;}
            const auto L=VoxelCoord::Unlinear(X.Key);VoxelGen::I3 P{O.Key.X*16+L.X,O.Key.Y*16+L.Y,O.Key.Z*16+L.Z};
            VoxelGen::Cell Symbol;
            if(!B.Contains(P)||!R.Generator->ToSymbol(X.Value,Symbol)){E=TEXT("Proxy block is absent from frozen catalog");return false;}
            Edits.push_back({P,Symbol});
            if(Edits.size()>131072){OverBudget=true;return true;}
        }
        return true;
    };
    for(const auto& X:Disk)
    {
        if(VoxelGen::Canceled(Cancel))return false;
        if(R.Resident.Contains(X.Key))continue; // EVEN AN EMPTY RESIDENT OVERLAY replaces the older saved record.
        FVoxelSectionOverlay O;if(!FVoxelDeltaCodec::Decode(X.Value,R.Manifest,*R.Registry,O)||!(O.Key==X.Key))
        {E=TEXT("Invalid saved proxy overlay");return false;}
        if(!Collect(O))return false;if(OverBudget)break;
    }
    if(!OverBudget)for(const auto& X:R.Resident){if(!Collect(X.Value))return false;if(OverBudget)break;}
    if(OverBudget){Reply.Mode=EVoxelProxyMode::Split;Out=MoveTemp(Reply);return true;}
    if(Edits.empty())Reply.Mode=EVoxelProxyMode::Natural;
    else
    {
        std::string Error;const auto& C=R.Generator->GetConfig();
        if(!VoxelView::BuildProxy(C.Settings.ToKernel(C.BlockSizeCentimeters),C.Catalog,R.Key,Edits,Reply.Grid,Error,Cancel))
        {E=UTF8_TO_TCHAR(Error.c_str());return false;}
        Reply.Grid.version=R.Version;Reply.Mode=EVoxelProxyMode::Grid;
    }
    Out=MoveTemp(Reply);E.Reset();return true;
}
bool FVoxelProxyData::EncodeRequest(const FVoxelProxyRequest& Q,TArray<uint8>& O)
{
    if(!ValidKey(Q.Key)||!Q.Serial)return false;FVoxelByteWriter W(64);W.U32(0x33515056);WriteKey(W,Q.Key);W.U64(Q.Serial);W.U64(Q.KnownVersion);return W.Finish(O);
}
bool FVoxelProxyData::DecodeRequest(TConstArrayView<uint8> B,FVoxelProxyRequest& O)
{
    if(B.Num()>64)return false;FVoxelByteReader R(B);if(R.U32()!=0x33515056)return false;FVoxelProxyRequest Q;Q.Key=ReadKey(R);Q.Serial=R.U64();Q.KnownVersion=R.U64();
    if(!R.End()||!ValidKey(Q.Key)||!Q.Serial)return false;O=Q;return true;
}
bool FVoxelProxyData::EncodeReply(const FVoxelProxyReply& Q,TArray<uint8>& O)
{
    if(!ValidKey(Q.Key)||!Q.Serial||uint8(Q.Mode)>4)return false;FVoxelByteWriter W(17000);W.U32(0x33525056);WriteKey(W,Q.Key);W.U64(Q.Serial);W.U64(Q.Version);W.U8(uint8(Q.Mode));
    if(Q.Mode==EVoxelProxyMode::Grid){W.U64(Q.Grid.hash);for(uint32 X:Q.Grid.cells)W.U32(X);}return W.Finish(O);
}
bool FVoxelProxyData::DecodeReply(TConstArrayView<uint8> B,uint32 Symbols,FVoxelProxyReply& O)
{
    if(B.Num()>17000)return false;FVoxelByteReader R(B);if(R.U32()!=0x33525056)return false;FVoxelProxyReply Q;Q.Key=ReadKey(R);Q.Serial=R.U64();Q.Version=R.U64();Q.Mode=EVoxelProxyMode(R.U8());
    if(!ValidKey(Q.Key)||!Q.Serial||uint8(Q.Mode)>4)return false;
    if(Q.Mode==EVoxelProxyMode::Grid)
    {
        Q.Grid.key=Q.Key;Q.Grid.version=Q.Version;Q.Grid.natural=false;Q.Grid.hash=R.U64();std::vector<uint8_t> Canonical;Canonical.reserve(16384);
        for(auto& X:Q.Grid.cells){X=R.U32();if(VoxelGen::Symbol(X)>=Symbols||(VoxelGen::Symbol(X)==0&&X!=0))return false;for(int I=0;I<4;++I)Canonical.push_back(uint8(X>>(I*8)));}
        if(VoxelGen::HashBytes(Canonical)!=Q.Grid.hash)return false;
    }
    if(!R.End())return false;O=MoveTemp(Q);return true;
}
