#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneModuleTypes.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"

class FVoxelGenerationPipeline;
class FVoxelTaskScheduler;
class UTexture2D;
struct FVoxelGenerationRuntimeConfig;
struct FVoxelTaskResult;

struct WHFRAMEWORK_API FVoxelMapTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	int32 Step = 16;

	bool operator==(const FVoxelMapTileKey& InOther) const
	{
		return Coordinate == InOther.Coordinate && Step == InOther.Step;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelMapTileKey& InKey)
{
	return HashCombineFast(GetTypeHash(InKey.Coordinate), GetTypeHash(InKey.Step));
}

/** Natural terrain raster tiles; construction and texture publication happen on the game thread. */
class WHFRAMEWORK_API FVoxelMapTileCache
{
public:
	static constexpr int32 TileSide = 64;

	FVoxelMapTileCache(
		FVoxelTaskScheduler& InScheduler,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		uint64 InWorldEpoch,
		double InBlockSize);

	static int32 SelectStep(float InCellPixelSize);
	static void VisibleTiles(
		const FSceneMapView& InView,
		const FVector2D& InPanelSize,
		double InBlockSize,
		TArray<FVoxelMapTileKey>& OutKeys);

	void Request(const TArray<FVoxelMapTileKey>& InKeys, const FSceneMapView& InView);
	const FSlateBrush* FindBrush(const FVoxelMapTileKey& InKey) const;
	uint64 GetRevision() const { return Revision; }

private:
	struct FTile
	{
		TStrongObjectPtr<UTexture2D> Texture;
		FSlateBrush Brush;
		uint64 LastUse = 0;
	};

	void ApplyTile(const FVoxelMapTileKey& InKey, FVoxelTaskResult&& InResult);
	void EvictOldest();

	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	uint64 WorldEpoch = 0;
	double BlockSize = 1.0;
	uint64 Sequence = 0;
	uint64 Revision = 0;
	TMap<FVoxelMapTileKey, FTile> Tiles;
	TSet<FVoxelMapTileKey> Pending;
};
