#pragma once
#include "CoreMinimal.h"
class UVoxelWorldGenerationProfile;
struct FVoxelRegistrySnapshot;
class WHFRAMEWORKEDITOR_API FVoxelGenerationCompiler
{
public:
    static bool Compile(UVoxelWorldGenerationProfile& Profile,const FVoxelRegistrySnapshot& Registry,
                        TArray<FString>& Report,FString& Error,bool bWriteAsset);
};
