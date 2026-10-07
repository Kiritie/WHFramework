#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveGameGenerationLease.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelTerrainResources.h"

struct WHFRAMEWORK_API FVoxelSceneActorRead
{
	FGuid Id;
	int32 Offset = 0;
	int32 Bytes = 0;
};

// Worker 只校验纯字节；反射和项目状态恢复由所属 Region 在 GT 分步执行。
struct WHFRAMEWORK_API FVoxelSceneColumnRead
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> SourceLease;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelBinaryBuffer Body;
	TArray<FVoxelSceneActorRead> Actors;
	int32 ProjectOffset = 0;
	int32 ProjectBytes = 0;

	static bool Load(const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& InSource,
		FIntPoint InColumn, uint32 InExpectedBytes,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe>& OutRead,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr, const TAtomic<bool>* InDetached = nullptr);
	static bool Validate(TConstArrayView<uint8> InBytes, FString& OutError);
	TConstArrayView<uint8> ActorPayload(int32 InIndex) const;
	TConstArrayView<uint8> ProjectPayload() const;
};
