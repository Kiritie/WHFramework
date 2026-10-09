#pragma once

#include "CoreMinimal.h"
#include "Voxel/Save/VoxelSceneColumnCapture.h"

class FVoxelTerrainResourceLease;
struct FVoxelWorldSaveCompletion;
struct FVoxelSceneColumnCapture;
class UVoxelSceneRegion;

struct WHFRAMEWORK_API FVoxelSceneSaveCut
{
	FGuid RegionInstanceId;
	uint64 Revision = 0;
};

// 与同一 SaveGame 文件代一起冻结；GT 只在实际 Worker 完成之后消费回执。
struct WHFRAMEWORK_API FVoxelSceneSaveCapture
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FGuid TransactionId;
	uint64 WorldEpoch = 0;
	TMap<FIntVector, FVoxelSceneSaveCut> Regions;
	TMap<FString, TArray<uint8>> Files;
	TMap<FString, TSharedPtr<const FVoxelSceneColumnCapture, ESPMode::ThreadSafe>> Columns;
	TSharedPtr<FVoxelWorldSaveCompletion, ESPMode::ThreadSafe> Completion;

	void VisitRequiredSources(const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
	{
		if (MetadataLease && MetadataLease->IsOwnedBy(InResources))
			FVoxelGenerationRequiredSourceScope::VisitLease(MetadataLease.Get(), InVisit);
		for (const auto& Pair : Columns)
			if (Pair.Value) Pair.Value->VisitRequiredSources(InResources, InVisit);
	}

	const TArray<uint8>* FindEncodedFile(const FString& InPath) const
	{
		if (const auto* Column = Columns.Find(InPath))
		{
			return *Column && (*Column)->Completion && (*Column)->Completion->bEncoded ? &(*Column)->Completion->Bytes : nullptr;
		}
		return Files.Find(InPath);
	}
};

// GT 的唯一捕获游标。只有封口后的纯字节根可以进入 SaveGame Worker。
struct WHFRAMEWORK_API FVoxelSceneSaveCaptureBuilder
{
	TSharedPtr<FVoxelSceneSaveCapture, ESPMode::ThreadSafe> Scenes;
	FVoxelSceneColumnCaptureCursor Column;
	TWeakObjectPtr<UVoxelSceneRegion> NextRegion;
	TWeakObjectPtr<UVoxelSceneRegion> CurrentRegion;
	FVoxelGenerationConsumerKey Consumer;
	FString CurrentFilePath;
	uint64 WorldEpoch = 0;
	uint64 SceneRevision = 0;
	uint64 ProjectRevision = 0;
	uint64 BlockedReleaseVersion = 0;
	uint64 MetadataBytes = 0;
	int32 Phase = 0;
	int32 FileCursor = 0;
	int32 FileOffset = 0;
	int32 RegionCount = 0;
	bool bCapacityBlocked = false;
};
