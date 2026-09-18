#pragma once
#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelRenderTypes.h"
class UMaterial;class UMaterialInstanceConstant;class UTexture2DArray;
class WHFRAMEWORKEDITOR_API FVoxelMaterialBuilder
{
public:
    static UMaterial* BuildMaster(const FString& PackageName,EVoxelRenderGroup Group,UTexture2DArray* DefaultArray,FString& Error);
    static UMaterialInstanceConstant* BuildInstance(const FString& PackageName,UMaterial* Parent,UTexture2DArray* Array,FString& Error);
};
