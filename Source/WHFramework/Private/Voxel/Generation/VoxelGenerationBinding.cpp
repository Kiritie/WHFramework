#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/Assets/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include "Voxel/Runtime/VoxelRegistry.h"
bool FVoxelGenerationBinding::Build(const UVoxelWorldGenerationProfile& P,const FVoxelRegistrySnapshot& R,const FVoxelGenerationSettings& S,int32 Size,FVoxelGenerationRuntimeConfig& O,FString& E)
{
    if(P.CatalogBakeVersion!=1||P.BakedCellCentimeters!=Size||P.CatalogBytes.IsEmpty()||P.CatalogBytes.Num()>64*1024*1024){E=TEXT("Generation profile is unbaked or has a different voxel size");return false;}
    std::vector<uint8_t> B(P.CatalogBytes.GetData(),P.CatalogBytes.GetData()+P.CatalogBytes.Num());
    if(VoxelGen::HashBytes(B)!=P.CatalogHash){E=TEXT("Generation catalog fingerprint is invalid");return false;}
    auto C=std::make_shared<VoxelGen::Catalog>();std::string Error;
    if(!VoxelGen::DecodeCatalog(B,*C,Error)||!C->Validate(S.ToKernel(Size),Error)){E=UTF8_TO_TCHAR(Error.c_str());return false;}
    FVoxelGenerationRuntimeConfig T;T.Settings=S;T.BlockSizeCentimeters=Size;T.CatalogHash=P.CatalogHash;
    T.SymbolToRuntime.resize(C->blocks.size());T.RuntimeToSymbol.assign(R.Definitions.Num(),MAX_uint16);
    for(uint32 I=0;I<C->blocks.size();++I)
    {
        const FName Name(UTF8_TO_TCHAR(C->blocks[I].c_str()));const auto* D=R.Find(Name);
        if(!D){E=FString::Printf(TEXT("Catalog block missing from Registry: %s"),*Name.ToString());return false;}
        T.SymbolToRuntime[I]=D->TypeId;T.RuntimeToSymbol[D->TypeId]=uint16(I);
    }
    auto Definition=[&](uint16 Symbol)->const FVoxelRuntimeDefinition*{return Symbol<T.SymbolToRuntime.size()?R.Find(T.SymbolToRuntime[Symbol]):nullptr;};
    auto Cube=[&](uint16 Symbol)->bool{const auto* D=Definition(Symbol);return D&&D->EntityKind==0&&D->Shape==EVoxelShapeKind::FullCube;};
    const auto& A=C->palette;
    const uint16 Cubes[]={A.stone,A.dirt,A.grass,A.sand,A.snow,A.bedrock,A.road};
    for(uint16 K:Cubes)if(!Cube(K)){E=TEXT("Terrain role must be a plain FullCube with no BlockEntity");return false;}
    for(uint16 K:{A.water,A.lava}){const auto* D=Definition(K);if(!D||D->Shape!=EVoxelShapeKind::Fluid||D->bSolid||D->EntityKind){E=TEXT("Fluid role has an invalid shape/physics/entity definition");return false;}}
    if(Definition(A.water)->RenderGroup!=EVoxelRenderGroup::Water||Definition(A.lava)->RenderGroup!=EVoxelRenderGroup::Emissive){E=TEXT("Water/Lava require Water/Emissive render groups");return false;}
    for(const auto& V:C->trees)if(!Cube(V.trunk)||!Cube(V.leaves)){E=TEXT("Tree trunk and crown must use plain shared cube definitions");return false;}
    for(const auto& V:C->plants){const auto* D=Definition(V.block);if(!D||D->Shape!=EVoxelShapeKind::CrossPlant||D->bSolid||D->EntityKind||D->DropCount!=0){E=TEXT("Phase2 ground vegetation requires non-solid/entity-free CrossPlant with DropCount=0");return false;}}
    for(const auto& V:C->ores)if(!Cube(V.block)){E=TEXT("Ore must be a plain cube");return false;}
    for(const auto& V:C->structures)for(const auto& Run:V.writes)if(VoxelGen::Symbol(Run.value)&&!Cube(VoxelGen::Symbol(Run.value))){E=TEXT("Baked structure contains a non-static cube or gameplay marker");return false;}
    T.Stone=T.SymbolToRuntime[A.stone];T.Dirt=T.SymbolToRuntime[A.dirt];T.Grass=T.SymbolToRuntime[A.grass];T.Sand=T.SymbolToRuntime[A.sand];T.Snow=T.SymbolToRuntime[A.snow];T.Water=T.SymbolToRuntime[A.water];T.Lava=T.SymbolToRuntime[A.lava];
    T.Catalog=C;O=MoveTemp(T);E.Reset();return true;
}
