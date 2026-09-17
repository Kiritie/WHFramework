#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
namespace
{
void Recipe(FVoxelByteWriter& W,const FVoxelWorldManifest& M)
{
    W.U32(FVoxelWorldManifest::GeneratorVersion);W.I32(M.BlockSizeCentimeters);W.U64(M.CatalogHash);
    W.I32(M.Settings.Seed);
    W.I32(M.Settings.MinZ);
    W.I32(M.Settings.MaxZ);
    W.I32(M.Settings.SeaLevel);
    W.I32(M.Settings.BaseHeight);
    W.I32(M.Settings.ContinentalPeriod);
    W.I32(M.Settings.ErosionPeriod);
    W.I32(M.Settings.MountainPeriod);
    W.I32(M.Settings.ClimatePeriod);
    W.I32(M.Settings.DetailPeriod);
    W.I32(M.Settings.ContinentalAmplitude);
    W.I32(M.Settings.MountainAmplitude);
    W.I32(M.Settings.DetailAmplitude);
    W.I32(M.Settings.RiverPeriod);
    W.I32(M.Settings.RiverWidthQ15);
    W.I32(M.Settings.RiverDepth);
    W.I32(M.Settings.LakeSpacing);
    W.I32(M.Settings.LakeRadius);
    W.I32(M.Settings.LakeDepth);
    W.I32(M.Settings.CavePeriod);
    W.I32(M.Settings.ChamberPeriod);
    W.I32(M.Settings.CaveWidthQ15);
    W.I32(M.Settings.ChamberThresholdQ15);
    W.I32(M.Settings.AquiferSpacing);
    W.I32(M.Settings.AquiferRadius);
    W.I32(M.Settings.LavaCeiling);
    W.I32(M.Settings.StructureSpacing);
    W.I32(M.Settings.StructureChancePermille);
    W.I32(M.Settings.MaxSiteCutFill);

}
bool ReadRecipe(FVoxelByteReader& R,FVoxelWorldManifest& M)
{
    if(R.U32()!=FVoxelWorldManifest::GeneratorVersion)return false;
    M.BlockSizeCentimeters=R.I32();M.CatalogHash=R.U64();
    M.Settings.Seed=R.I32();
    M.Settings.MinZ=R.I32();
    M.Settings.MaxZ=R.I32();
    M.Settings.SeaLevel=R.I32();
    M.Settings.BaseHeight=R.I32();
    M.Settings.ContinentalPeriod=R.I32();
    M.Settings.ErosionPeriod=R.I32();
    M.Settings.MountainPeriod=R.I32();
    M.Settings.ClimatePeriod=R.I32();
    M.Settings.DetailPeriod=R.I32();
    M.Settings.ContinentalAmplitude=R.I32();
    M.Settings.MountainAmplitude=R.I32();
    M.Settings.DetailAmplitude=R.I32();
    M.Settings.RiverPeriod=R.I32();
    M.Settings.RiverWidthQ15=R.I32();
    M.Settings.RiverDepth=R.I32();
    M.Settings.LakeSpacing=R.I32();
    M.Settings.LakeRadius=R.I32();
    M.Settings.LakeDepth=R.I32();
    M.Settings.CavePeriod=R.I32();
    M.Settings.ChamberPeriod=R.I32();
    M.Settings.CaveWidthQ15=R.I32();
    M.Settings.ChamberThresholdQ15=R.I32();
    M.Settings.AquiferSpacing=R.I32();
    M.Settings.AquiferRadius=R.I32();
    M.Settings.LavaCeiling=R.I32();
    M.Settings.StructureSpacing=R.I32();
    M.Settings.StructureChancePermille=R.I32();
    M.Settings.MaxSiteCutFill=R.I32();

    std::string E;return R.IsValid()&&M.CatalogHash&&M.Settings.ToKernel(M.BlockSizeCentimeters).Validate(E);
}
}
uint64 FVoxelManifestCodec::RecipeFingerprint(const FVoxelWorldManifest& M)
{
    FVoxelByteWriter W(MaxBytes);Recipe(W,M);TArray<uint8> B;if(!W.Finish(B))return 0;return VoxelBinary::Hash(B);
}
bool FVoxelManifestCodec::Encode(const FVoxelWorldManifest& M,TArray<uint8>& O)
{
    std::string E;if(!M.WorldId.IsValid()||!M.CatalogHash||!M.Settings.ToKernel(M.BlockSizeCentimeters).Validate(E))return false;
    FVoxelByteWriter W(MaxBytes);W.U32(0x334d5856);W.Guid(M.WorldId);Recipe(W,M);W.U64(M.RegistryHash);W.U64(RecipeFingerprint(M));W.U64(M.BaseSampleHash);return W.Finish(O);
}
bool FVoxelManifestCodec::Decode(TConstArrayView<uint8> B,FVoxelWorldManifest& O)
{
    if(B.Num()>MaxBytes)return false;FVoxelByteReader R(B);if(R.U32()!=0x334d5856)return false;
    FVoxelWorldManifest M;M.WorldId=R.Guid();if(!M.WorldId.IsValid()||!ReadRecipe(R,M))return false;
    M.RegistryHash=R.U64();M.RecipeHash=R.U64();M.BaseSampleHash=R.U64();if(!R.End()||M.RecipeHash!=RecipeFingerprint(M))return false;O=M;return true;
}
