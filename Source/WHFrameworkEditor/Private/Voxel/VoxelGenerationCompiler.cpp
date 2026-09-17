#include "Voxel/VoxelGenerationCompiler.h"
#include "Voxel/Generation/Assets/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include <map>
#include <set>
namespace
{
using namespace VoxelGen;
I3 ToI(FIntVector P){return {P.X,P.Y,P.Z};}
Box ToB(const FVoxelGridBox& B){return {ToI(B.Min),ToI(B.Max)};}
std::string Text(const FString& S){return TCHAR_TO_UTF8(*S);}
uint32_t Mask(const TArray<EVoxelBiomeId>& A)
{
    if(A.IsEmpty())return 0xffffffffu;uint32_t M=0;
    for(auto B:A)if(uint8(B)<32)M|=uint32_t(1)<<uint8(B);return M;
}
struct FCompiler
{
    const FVoxelRegistrySnapshot& R;Catalog C;FVoxelShapeRegistry Shapes;
    TMap<FName,uint16> Symbols;TArray<FString>& Report;FString& E;int32 CellCm;
    uint16 Block(const UVoxelData* A)
    {
        if(!A||!A->bRegisterBlock||!R.Find(A->GetPrimaryAssetId())){E=TEXT("Missing/unregistered block asset in generation profile");return 0;}
        if(auto* P=Symbols.Find(A->BlockName))return *P;
        if(C.blocks.size()>=65535){E=TEXT("Too many generation symbols");return 0;}
        uint16 ID=uint16(C.blocks.size());C.blocks.push_back(Text(A->BlockName.ToString()));Symbols.Add(A->BlockName,ID);return ID;
    }
    uint16 Block(TSoftObjectPtr<UVoxelData> A){return Block(A.LoadSynchronous());}
    bool Structure(UVoxelStructureData& A)
    {
        auto* P=A.SourcePrefab.LoadSynchronous();
        if(!P||A.StableKey.IsNone()||A.SourceCellCentimeters<CellCm||A.SourceCellCentimeters%CellCm||
           A.SourceCellCentimeters/CellCm>8){E=TEXT("Structure needs source prefab, stable key and integral source/target scale in [1,8]");return false;}
        const int32 Scale=A.SourceCellCentimeters/CellCm;
        StructureTemplate T;T.key=Text(A.StableKey.ToString());T.kind=StructureKind(uint8(A.Kind));
        T.sourceCellCm=CellCm;T.groundZ=A.SourceGroundCellZ*Scale+Scale-1;
        T.bounds=ToB(A.SourceBounds);T.bounds.min=T.bounds.min*Scale;T.bounds.max=T.bounds.max*Scale;
        T.entrance=ToI(A.SourceEntranceCorner)*Scale;
        for(const auto& B:A.ClearVolumes){Box D=ToB(B);D.min=D.min*Scale;D.max=D.max*Scale;T.clear.push_back(D);}
        std::map<std::tuple<int,int,int>,Cell> Cells;
        std::set<I3> Seen;
        if(P->Data.Cells.Num()>1048576){E=TEXT("Source prefab exceeds one million authored cells");return false;}
        for(const auto& X:P->Data.Cells)
        {
            const auto* Def=R.Find(X.Item.VoxelAssetID);
            const I3 Source=ToI(X.Offset);
            if(!Def||X.Item.Count!=1||X.Item.State<0||X.Item.State>65535||!ToB(A.SourceBounds).Contains(Source)||!Seen.insert(Source).second)
            {E=TEXT("Source prefab has invalid/duplicate cells or mismatched declared bounds");return false;}
            if(Def->EntityKind==100){Report.Add(FString::Printf(TEXT("STRIP_SPAWNER %s %s"),*A.GetPathName(),*X.Offset.ToString()));continue;}
            // Resolve by explicit references in mappings or already registered primary asset object.
            UVoxelData* Replacement=nullptr;
            for(const auto& M:A.StaticMappings)
            {
                auto* From=M.Source.LoadSynchronous();
                if(From&&From->GetPrimaryAssetId()==X.Item.VoxelAssetID){Replacement=M.SolidReplacement.LoadSynchronous();break;}
            }
            uint16 SymbolID=0;
            if(Replacement)
            {
                const auto* D=R.Find(Replacement->GetPrimaryAssetId());
                if(!D||D->Shape!=EVoxelShapeKind::FullCube||D->EntityKind){E=TEXT("Static replacement must be an entity-free FullCube");return false;}
                SymbolID=Block(Replacement);
            }
            else if(Def->Shape==EVoxelShapeKind::FullCube&&Def->EntityKind==0)
            {
                if(const auto* S=Symbols.Find(Def->BlockName))SymbolID=*S;
                else{if(C.blocks.size()>=65535)return false;SymbolID=uint16(C.blocks.size());Symbols.Add(Def->BlockName,SymbolID);C.blocks.push_back(Text(Def->BlockName.ToString()));}
            }
            else {E=FString::Printf(TEXT("Explicit pure-cube replacement required: %s in %s"),*Def->BlockName.ToString(),*A.GetPathName());return false;}
            if(!SymbolID)return false;
            uint16 State=uint16(X.Item.State);
            if(A.bBakeDoorsOpen&&(Def->Shape==EVoxelShapeKind::Door||Def->Shape==EVoxelShapeKind::Trapdoor))State|=VoxelState::OpenMask;
            const auto* Shape=Shapes.Find(Def->Shape,State);
            if(!Shape||Shape->SelectionBoxes.IsEmpty()){E=TEXT("Structure source needs volume geometry, not cross-plant/decal; use a Detail socket");return false;}
            // One source cell owns its entire refined cube, including the holes in stairs/open doors.
            for(int Z=0;Z<Scale;++Z)for(int Y=0;Y<Scale;++Y)for(int Xp=0;Xp<Scale;++Xp)
            {
                const FVector Point((Xp+.5)/Scale,(Y+.5)/Scale,(Z+.5)/Scale);bool Inside=false;
                for(const FBox& B:Shape->SelectionBoxes)Inside|=B.IsInsideOrOn(Point);
                I3 Target=Source*Scale+I3{Xp,Y,Z};
                Cell Value=Inside?Pack(SymbolID,Def->Shape==EVoxelShapeKind::FullCube?uint16(State&3):0):0;
                Cells[{Target.z,Target.y,Target.x}]=Value;
                if(Cells.size()>8u*1024u*1024u){E=TEXT("Refined source exceeds eight million cells; split the prefab into authored modules");return false;}
            }
        }
        for(const auto& X:Cells)
        {
            const auto [Z,Y,Xp]=X.first;
            if(!T.writes.empty())
            {
                auto& Last=T.writes.back();
                if(Last.start.z==Z&&Last.start.y==Y&&Last.start.x+Last.length==Xp&&Last.value==X.second){++Last.length;continue;}
            }
            T.writes.push_back({{Xp,Y,Z},1,X.second});
        }
        for(const auto& D:A.Details)
        {
            auto* Asset=D.Detail.LoadSynchronous();if(!Asset||D.Yaw<0||D.Yaw>3){E=TEXT("Invalid detail socket");return false;}
            auto It=std::find_if(C.details.begin(),C.details.end(),[&](const auto& Q){return Q.assetPath==Text(Asset->GetPathName());});
            if(It==C.details.end()){E=TEXT("All structure details must be listed in profile Details");return false;}
            T.details.push_back({uint32_t(It-C.details.begin()),ToI(D.SourceCorner)*Scale,uint8_t(D.Yaw)});
        }
        Report.Add(FString::Printf(TEXT("STRUCTURE %s source=%dcm target=%dcm runs=%llu"),*A.GetPathName(),A.SourceCellCentimeters,CellCm,uint64(T.writes.size())));
        C.structures.push_back(MoveTemp(T));return true;
    }
};
}
bool FVoxelGenerationCompiler::Compile(UVoxelWorldGenerationProfile& P,const FVoxelRegistrySnapshot& R,TArray<FString>& Report,FString& E,bool Write)
{
    check(IsInGameThread());E.Reset();FCompiler B{R,{}, {},{},Report,E,P.TargetCellCentimeters};B.Shapes.BuildDefaults();B.C.blocks={"core:air"};B.Symbols.Add(TEXT("core:air"),0);
    for(const auto& D:R.Definitions)if(D.TypeId){if(B.C.blocks.size()>=65535){E=TEXT("Registry symbol budget exceeded");return false;}B.Symbols.Add(D.BlockName,uint16(B.C.blocks.size()));B.C.blocks.push_back(Text(D.BlockName.ToString()));}
    const TSoftObjectPtr<UVoxelData> Roles[]={P.Stone,P.Dirt,P.Grass,P.Sand,P.Snow,P.Water,P.Lava,P.Bedrock,P.Road};
    uint16* Targets[]={&B.C.palette.stone,&B.C.palette.dirt,&B.C.palette.grass,&B.C.palette.sand,&B.C.palette.snow,&B.C.palette.water,&B.C.palette.lava,&B.C.palette.bedrock,&B.C.palette.road};
    for(int32 I=0;I<9;++I)if(!(*Targets[I]=B.Block(Roles[I])))return false;
    for(const auto& Soft:P.Details)
    {
        auto* D=Soft.LoadSynchronous();if(!D||!D->Validate(E))return false;
        B.C.details.push_back({Text(D->StableKey.ToString()),Text(D->GetPathName()),D->GeometryHash,ToB(D->FootprintCells)});
    }
    for(const auto& A:P.Trees)
    {
        TreeRule T;T.key=Text(A.Key.ToString());T.kind=TreeKind(uint8(A.Form));T.biomeMask=Mask(A.Biomes);T.trunk=B.Block(A.Trunk);T.leaves=B.Block(A.Leaves);
        T.spacing=P.TreeSpacing;T.minHeight=A.MinHeight;T.maxHeight=A.MaxHeight;T.trunkRadius=A.TrunkRadius;T.crownRadius=A.CrownRadius;T.chancePermille=A.ChancePermille;T.maxSlope=A.MaxSlope;B.C.trees.push_back(T);
    }
    for(const auto& A:P.Plants)B.C.plants.push_back({Text(A.Key.ToString()),Mask(A.Biomes),B.Block(A.Block),A.ChancePermille,A.PatchPeriod});
    for(const auto& A:P.Ores)B.C.ores.push_back({Text(A.Key.ToString()),B.Block(A.Block),A.MinZ,A.MaxZ,A.Spacing,A.Radius,A.Length,A.ChancePermille,Mask(A.Biomes)});
    for(const auto& Soft:P.Structures){auto* A=Soft.LoadSynchronous();if(!A||!B.Structure(*A))return false;}
    if(!E.IsEmpty())return false;
    auto Settings=P.Defaults.ToKernel(P.TargetCellCentimeters);std::string Error;std::vector<uint8_t> Bytes;
    if(!B.C.Validate(Settings,Error)||!EncodeCatalog(B.C,Bytes,Error)){E=UTF8_TO_TCHAR(Error.c_str());return false;}
    TArray<uint8> Out;Out.Append(Bytes.data(),int32(Bytes.size()));
    // Validate runtime role/shape restrictions before changing the profile.
    auto OldBytes=P.CatalogBytes;auto OldHash=P.CatalogHash;int32 OldVersion=P.CatalogBakeVersion,OldCell=P.BakedCellCentimeters;
    P.CatalogBytes=Out;P.CatalogHash=HashBytes(Bytes);P.CatalogBakeVersion=1;P.BakedCellCentimeters=P.TargetCellCentimeters;
    FVoxelGenerationRuntimeConfig Bound;const bool OK=FVoxelGenerationBinding::Build(P,R,P.Defaults,P.TargetCellCentimeters,Bound,E);
    if(!OK||!Write)
    {
        const bool BakedMatches=OldVersion==1&&OldCell==P.TargetCellCentimeters&&OldHash==P.CatalogHash&&OldBytes==Out;
        P.CatalogBytes=MoveTemp(OldBytes);P.CatalogHash=OldHash;P.CatalogBakeVersion=OldVersion;P.BakedCellCentimeters=OldCell;
        if(OK&&!Write&&!BakedMatches){E=TEXT("Profile authoring content differs from its baked catalog; run explicit bake");return false;}
        return OK;
    }
    P.MarkPackageDirty();return FVoxelEditorAssetIO::Save(&P,E);
}
