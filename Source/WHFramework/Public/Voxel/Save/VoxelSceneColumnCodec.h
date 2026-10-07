#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"

struct WHFRAMEWORK_API FVoxelSavedSceneActor
{
	FGuid Id;
	FParameter Data;
};

struct FVoxelSceneColumnCapture;
struct FVoxelGenerationConsumerKey;

class WHFRAMEWORK_API FVoxelSceneColumnCodec
{
public:
	static FString RelativePath(FIntPoint InColumn);
	static bool Encode(
		const TArray<FVoxelSavedSceneActor>& InActors,
		const FParameter& InProjectData,
		TArray<uint8>& OutBytes,
		FString& OutError);
	static bool Decode(
		TConstArrayView<uint8> InBytes,
		TArray<FVoxelSavedSceneActor>& OutActors,
		FParameter& OutProjectData,
		FString& OutError);
	static bool EncodePayloads(const FVoxelSceneColumnCapture& InCapture, FString& OutError,
		const FVoxelGenerationConsumerKey* InEncodingConsumer = nullptr);
};
