#pragma once
#include "CoreMinimal.h"
class UVoxelDetailData;
class UStaticMesh;
class WHFRAMEWORKEDITOR_API FVoxelDetailBaker
{
public:
    static bool Bake(UVoxelDetailData& Asset,const FString& GeneratedRoot,FString& Error);
    // Rebuild an already verified generated mesh without saving either package.
    static bool RebuildExistingInMemory(UVoxelDetailData& Asset, UStaticMesh& Mesh, const FString& GeneratedRoot, FString& Error);
};
