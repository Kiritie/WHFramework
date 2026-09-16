#pragma once
#include "CoreMinimal.h"
class UVoxelData;
class WHFRAMEWORKEDITOR_API FVoxelAssetMigrationTool
{
public:
	static bool ApplyPlan(const FString& JSONPath, bool bWrite, TArray<UVoxelData*>& OutAssets, FString& Error);
	static bool Scan(TArray<UVoxelData*>& OutAssets, FString& Error);
};
