#pragma once
#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
class UVoxelWorldGenerationProfile;
struct FVoxelRegistrySnapshot;
class WHFRAMEWORK_API FVoxelGenerationBinding
{
public:
    static bool Build(const UVoxelWorldGenerationProfile& Profile,const FVoxelRegistrySnapshot& Registry,const FVoxelGenerationSettings& Settings,int32 CellCm,FVoxelGenerationRuntimeConfig& Out,FString& Error);
};
