#include "Voxel/Generation/VoxelGenerationContext.h"
VoxelGen::Settings FVoxelGenerationSettings::ToKernel(int32 CellCm) const
{
    VoxelGen::Settings S;S.cellCm=CellCm;
    S.seed=Seed;
    S.minZ=MinZ;
    S.maxZ=MaxZ;
    S.sea=SeaLevel;
    S.base=BaseHeight;
    S.continentPeriod=ContinentalPeriod;
    S.erosionPeriod=ErosionPeriod;
    S.mountainPeriod=MountainPeriod;
    S.climatePeriod=ClimatePeriod;
    S.detailPeriod=DetailPeriod;
    S.continentAmplitude=ContinentalAmplitude;
    S.mountainAmplitude=MountainAmplitude;
    S.detailAmplitude=DetailAmplitude;
    S.riverPeriod=RiverPeriod;
    S.riverWidthQ15=RiverWidthQ15;
    S.riverDepth=RiverDepth;
    S.lakeSpacing=LakeSpacing;
    S.lakeRadius=LakeRadius;
    S.lakeDepth=LakeDepth;
    S.cavePeriod=CavePeriod;
    S.chamberPeriod=ChamberPeriod;
    S.caveWidthQ15=CaveWidthQ15;
    S.chamberThresholdQ15=ChamberThresholdQ15;
    S.aquiferSpacing=AquiferSpacing;
    S.aquiferRadius=AquiferRadius;
    S.lavaCeiling=LavaCeiling;
    S.structureSpacing=StructureSpacing;
    S.structureChancePermille=StructureChancePermille;
    S.maxSiteCutFill=MaxSiteCutFill;
    return S;
}
bool FVoxelGenerationSettings::Validate(FString& Error) const
{
    std::string E;const bool OK=ToKernel(25).Validate(E);Error=UTF8_TO_TCHAR(E.c_str());return OK;
}
