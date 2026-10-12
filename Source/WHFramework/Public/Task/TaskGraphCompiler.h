#pragma once

#include "CoreMinimal.h"

class UTaskAsset;
class UTaskBase;

class WHFRAMEWORK_API FTaskGraphCompiler
{
public:
	static bool Compile(UTaskAsset* Asset, TArray<FText>& OutErrors);
	static bool Validate(const UTaskAsset* Asset, TArray<FText>& OutErrors);

private:
	struct FCompiledTree
	{
		TArray<UTaskBase*> Roots;
		TMap<UTaskBase*, TArray<UTaskBase*>> Children;
		TMap<FString, UTaskBase*> Tasks;
	};

	static bool Build(const UTaskAsset* Asset, FCompiledTree& OutTree, TArray<FText>& OutErrors);
	static bool ValidateAsset(const UTaskAsset* Asset, TSet<const UTaskAsset*>& ActiveAssets, TSet<const UTaskAsset*>& ValidatedAssets, TArray<FText>& OutErrors);
	static void AddError(const UTaskAsset* Asset, const FString& Message, TArray<FText>& OutErrors);
};
