#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Chunks/VoxelSectionCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
bool FVoxelGenerationPipeline::BuildQuery(const VoxelGen::Box& B,bool Visual,std::unique_ptr<VoxelGen::Query>& O,FString& E,const std::atomic_bool* Cancel) const
{
    if(!Config.Catalog){E=TEXT("Generation catalog is not bound");return false;}
    auto Q=std::make_unique<VoxelGen::Query>(Config.Settings.ToKernel(Config.BlockSizeCentimeters),Config.Catalog);std::string Error;
    if(!Q->Prepare(B,Visual,Error,Cancel)){E=UTF8_TO_TCHAR(Error.c_str());return false;}
    O=std::move(Q);E.Reset();return true;
}
bool FVoxelGenerationPipeline::ToRuntime(VoxelGen::Cell S,FVoxelBlockState& O) const
{
    const uint16 Symbol=VoxelGen::Symbol(S);if(Symbol>=Config.SymbolToRuntime.size())return false;
    O={Config.SymbolToRuntime[Symbol],VoxelGen::State(S)};return true;
}
bool FVoxelGenerationPipeline::ToSymbol(FVoxelBlockState S,VoxelGen::Cell& O) const
{
    if(S.TypeId>=Config.RuntimeToSymbol.size()||Config.RuntimeToSymbol[S.TypeId]==MAX_uint16)return false;
    O=VoxelGen::Pack(Config.RuntimeToSymbol[S.TypeId],S.State);return true;
}
bool FVoxelGenerationPipeline::GenerateSection(const FVoxelSectionKey& K,FVoxelSectionStorage& O,const std::atomic_bool* Cancel) const
{
    if(!VoxelCoord::IsValidSection(K,Config.Settings.MinZ,Config.Settings.MaxZ))return false;
    const FIntVector P=VoxelCoord::Origin(K);
    const VoxelGen::Box B={{P.X,P.Y,FMath::Max(P.Z,Config.Settings.MinZ)},{P.X+16,P.Y+16,FMath::Min(P.Z+16,Config.Settings.MaxZ)}};
    std::unique_ptr<VoxelGen::Query> Q;FString E;if(!BuildQuery(B,false,Q,E,Cancel))return false;
    TArray<uint32> Dense;Dense.SetNumUninitialized(4096);
    for(int32 Y=0;Y<16;++Y)for(int32 X=0;X<16;++X)
    {
        if(VoxelGen::Canceled(Cancel))return false;
        for(int32 Z=0;Z<16;++Z){FVoxelBlockState Value;if(!ToRuntime(Q->Sample({P.X+X,P.Y+Y,P.Z+Z}),Value))return false;Dense[X+16*Y+256*Z]=Value.Pack();}
    }
    FVoxelSectionStorage T;if(!T.BuildFromDense(Dense))return false;O=MoveTemp(T);return true;
}
bool FVoxelGenerationPipeline::SampleBaseBlock(const FIntVector& P,FVoxelBlockState& O) const
{
    if(!VoxelCoord::IsValid(P))return false;
    if(P.Z<Config.Settings.MinZ||P.Z>=Config.Settings.MaxZ){O={};return true;}
    std::unique_ptr<VoxelGen::Query> Q;FString E;
    if(!BuildQuery({{P.X,P.Y,P.Z},{P.X+1,P.Y+1,P.Z+1}},false,Q,E))return false;
    return ToRuntime(Q->Sample({P.X,P.Y,P.Z}),O);
}
FVoxelColumnSample FVoxelGenerationPipeline::SampleColumn(int32 X,int32 Y) const
{
    FVoxelColumnSample O;if(!Config.Catalog)return O;
    const auto S=Config.Settings.ToKernel(Config.BlockSizeCentimeters);VoxelGen::Terrain T(S,Config.Catalog->palette);const auto C=T.SampleColumn(X,Y);
    O.SurfaceZ=C.height;O.HeightQ16=int64(C.height)*65536;O.Biome=static_cast<EVoxelBiomeId>(C.biome);O.TemperatureQ15=C.temperature;O.MoistureQ15=C.moisture;O.SlopePermille=C.slopePermille;O.WaterZ=C.water;O.bRiver=C.river;O.bLake=C.lake;O.bCoast=C.coast;return O;
}
bool FVoxelGenerationPipeline::BuildHandshakeSignature(uint64& O) const
{
    FVoxelByteWriter W(128);const int32 Z=VoxelCoord::FloorDiv(Config.Settings.BaseHeight,16);
    const FVoxelSectionKey Keys[]={{0,0,Z},{-1,-1,Z},{37,-91,Z},{-307,511,Z}};
    for(const auto& K:Keys){FVoxelSectionStorage S;if(!GenerateSection(K,S))return false;W.U64(FVoxelSectionCodec::Fingerprint(S));}
    W.U64(Config.CatalogHash);TArray<uint8> B;if(!W.Finish(B))return false;O=VoxelBinary::Hash(B);return true;
}
