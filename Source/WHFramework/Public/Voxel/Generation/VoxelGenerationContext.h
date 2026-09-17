#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Generation/Kernel/VoxelGenCatalog.h"
#include "VoxelGenerationContext.generated.h"
UENUM(BlueprintType)
enum class EVoxelBiomeId : uint8
{
	None,
	Plains,
	Forest,
	Desert,
	Snow,
	Mountain,
	Ocean,
	Savanna,
	Rainforest,
	Wetland,
	Badlands
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGenerationSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 Seed = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 MinZ = -512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 MaxZ = 2048;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 SeaLevel = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 BaseHeight = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ContinentalPeriod = 16000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ErosionPeriod = 8000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 MountainPeriod = 4000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ClimatePeriod = 24000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 DetailPeriod = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ContinentalAmplitude = 384;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 MountainAmplitude = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 DetailAmplitude = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 RiverPeriod = 3000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 RiverWidthQ15 = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 RiverDepth = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 LakeSpacing = 768;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 LakeRadius = 96;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 LakeDepth = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 CavePeriod = 96;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ChamberPeriod = 192;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 CaveWidthQ15 = 2400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 ChamberThresholdQ15 = 21000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 AquiferSpacing = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 AquiferRadius = 28;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 LavaCeiling = -320;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 StructureSpacing = 3072;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 StructureChancePermille = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Generation")
    int32 MaxSiteCutFill = 16;

    bool Validate(FString& Error) const;
    VoxelGen::Settings ToKernel(int32 CellCm) const;
};
struct WHFRAMEWORK_API FVoxelGenerationRuntimeConfig
{
    FVoxelGenerationSettings Settings;
    int32 BlockSizeCentimeters=25;
    FVoxelTypeId Stone=0,Dirt=0,Grass=0,Sand=0,Snow=0,Water=0,Lava=0;
    uint64 CatalogHash=0;
    std::shared_ptr<const VoxelGen::Catalog> Catalog;
    std::vector<uint16_t> SymbolToRuntime;
    std::vector<uint16_t> RuntimeToSymbol;
};
struct WHFRAMEWORK_API FVoxelColumnSample
{
    int64 HeightQ16=0;
    int32 SurfaceZ=0;
    EVoxelBiomeId Biome=EVoxelBiomeId::Plains;
    int32 TemperatureQ15=0,MoistureQ15=0;
    int32 WaterZ=MIN_int32;
    bool bRiver=false,bLake=false,bCoast=false;
};
struct WHFRAMEWORK_API FVoxelWorldManifest
{
    static constexpr uint32 ProtocolVersion=3;
    static constexpr uint32 GeneratorVersion=VoxelGen::AlgorithmVersion;
    FGuid WorldId;
    FVoxelGenerationSettings Settings;
    int32 BlockSizeCentimeters=25;
    uint64 RegistryHash=0,CatalogHash=0,RecipeHash=0,BaseSampleHash=0;
};
