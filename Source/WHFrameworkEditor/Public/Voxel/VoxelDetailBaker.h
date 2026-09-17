#pragma once
#include "CoreMinimal.h"
class UVoxelDetailData;
class WHFRAMEWORKEDITOR_API FVoxelDetailBaker
{
public:
    static bool Bake(UVoxelDetailData& Asset,const FString& GeneratedRoot,FString& Error);
};
