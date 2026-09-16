#pragma once
#include "CoreMinimal.h"
class UPackage;
class WHFRAMEWORKEDITOR_API FVoxelEditorAssetIO
{
public:
	static UObject* LoadOrCreate(UClass* Class, const FString& PackageName, FString& Error);
	static bool Save(UObject* Asset, FString& Error);
};
