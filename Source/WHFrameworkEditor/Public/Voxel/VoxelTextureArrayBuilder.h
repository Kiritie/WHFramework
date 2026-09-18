#pragma once
#include "CoreMinimal.h"
class UVoxelData;class UVoxelMaterialSet;
struct FVoxelArrayBuildOptions
{
    FString OutputRoot=TEXT("/Game/VoxelGenerated");
    int32 TileSize=16;
    int32 MaxSlicesPerBank=256;
};
class WHFRAMEWORKEDITOR_API FVoxelTextureArrayBuilder
{
public:
    static bool Build(const TArray<UVoxelData*>& Assets,UVoxelMaterialSet& MaterialSet,const FVoxelArrayBuildOptions& Options,FString& Error);
};
