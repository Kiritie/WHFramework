#include "Voxel/Generators/VoxelRegionGenerator.h"

#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRegionGenerator::UVoxelRegionGenerator()
{
	RegionSizeChunks = 32;
}

void UVoxelRegionGenerator::Generate(UVoxelChunk* InChunk)
{
	const int32 RegionSize = FMath::Max(RegionSizeChunks, 32);
	const FIndex ChunkIndex = InChunk->GetIndex();
	const FIndex Anchor(FMath::FloorToInt(static_cast<float>(ChunkIndex.X) / RegionSize) * RegionSize,
		FMath::FloorToInt(static_cast<float>(ChunkIndex.Y) / RegionSize) * RegionSize, 0);
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex RegionOrigin = Module->ChunkIndexToVoxelIndex(Anchor);
	const FVector2D RegionMin(RegionOrigin.X, RegionOrigin.Y);
	const FVector2D RegionMax(RegionOrigin.X + ChunkSize.X * RegionSize - 1, RegionOrigin.Y + ChunkSize.Y * RegionSize - 1);

	FSceneArea SceneArea;
	SceneArea.AreaName = *FString::Printf(TEXT("VoxelArea_%d_%d"), Anchor.X, Anchor.Y);
	SceneArea.AreaDisplayName = Module->GetWorldAreaPrefix(RegionOrigin, EVoxelSceneAreaNameType::Continent);
	SceneArea.AreaType = ESceneAreaType::Chunk;
	SceneArea.AreaShape = ESceneAreaShape::Box;
	SceneArea.AreaCenter = (RegionMin + RegionMax) * 0.5f;
	SceneArea.AreaRadius = (RegionMax - RegionMin) * 0.5f;
	USceneModuleStatics::AddSceneArea(SceneArea, true);
}
