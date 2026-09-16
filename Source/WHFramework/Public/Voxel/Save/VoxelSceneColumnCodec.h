#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"
struct WHFRAMEWORK_API FVoxelSavedSceneActor
{
	FGuid Id;
	FParameter Data;
};
class WHFRAMEWORK_API FVoxelSceneColumnCodec
{
public:
	static FString RelativePath(FIntPoint Column);
	static bool Encode(const TArray<FVoxelSavedSceneActor>& Actors, const FParameter& ProjectData, TArray<uint8>& Out, FString& Error);
	static bool Decode(TConstArrayView<uint8> Bytes, TArray<FVoxelSavedSceneActor>& Out, FParameter& ProjectData, FString& Error);
};
