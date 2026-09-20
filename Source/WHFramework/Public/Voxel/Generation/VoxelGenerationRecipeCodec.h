#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelGenerationRecipeCodec
{
public:
	static bool Encode(const FVoxelGenerationRecipe& InRecipe, TArray<uint8>& OutBytes, FString& OutError);
	static bool Decode(TConstArrayView<uint8> InBytes, FVoxelGenerationRecipe& OutRecipe, FString& OutError);
	static bool RefreshHash(FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static uint64 Hash(TConstArrayView<uint8> InBytes);
};
