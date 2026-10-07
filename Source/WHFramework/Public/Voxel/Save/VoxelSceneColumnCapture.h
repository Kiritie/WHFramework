#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Parameter/ParameterTypes.h"

struct WHFRAMEWORK_API FVoxelSceneActorPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FGuid Id;
	TArray<uint8> Bytes;
};

struct WHFRAMEWORK_API FVoxelSceneColumnCompletion
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TArray<uint8> Bytes;
	bool bEncoded = false;
};

// GT 已完成 UObject/反射访问；Worker 只读取普通 GUID 与序列化后的不可变字节。
struct WHFRAMEWORK_API FVoxelSceneColumnCapture
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TArray<FVoxelSceneActorPayload> Actors;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ProjectLease;
	TArray<uint8> ProjectBytes;
	TSharedPtr<FVoxelSceneColumnCompletion, ESPMode::ThreadSafe> Completion;
};

enum class EVoxelSceneCaptureStep : uint8
{
	Pending,
	CapacityBlocked,
	Complete,
	Failed
};

// 唯一 GT producer 的游标；发布后只将 Capture 的纯字节根交给 Worker。
struct WHFRAMEWORK_API FVoxelSceneColumnCaptureCursor
{
	TSharedPtr<FVoxelSceneColumnCapture, ESPMode::ThreadSafe> Capture;
	FGuid RegionInstanceId;
	uint64 WorldEpoch = 0;
	uint64 Revision = 0;
	uint64 ProjectRevision = MAX_uint64;
	uint64 BlockedReleaseVersion = 0;
	int32 Phase = 0;
	int32 PendingCursor = 0;
	int32 ActorCursor = 0;
	int32 ProjectCursor = 0;
	int32 ActorCapacity = 0;
	FParameter ProjectData;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ProjectStateLease;
	bool bCapacityBlocked = false;
};
