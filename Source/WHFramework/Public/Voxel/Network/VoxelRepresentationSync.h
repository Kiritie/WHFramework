#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

class UVoxelModule;

struct WHFRAMEWORK_API FVoxelRepresentationBuildInput
{
	FVoxelRepresentationRequest Request;
	FVoxelGenerationSettings Settings;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	FVoxelOverlaySnapshotSet Overlays;
	uint64 Revision = 0;
};

class WHFRAMEWORK_API FVoxelRepresentationSync
{
public:
	static bool PrepareServerBuild(
		const UVoxelModule& InModule,
		const FVoxelRepresentationRequest& InRequest,
		FVoxelRepresentationBuildInput& OutInput,
		FString& OutError);
	static bool BuildServerData(
		const FVoxelRepresentationBuildInput& InInput,
		TArray<uint8>& OutBytes,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);
	static bool EncodeVoxelProxy(
		const FVoxelVoxelProxyData& InData,
		TArray<uint8>& OutBytes,
		FString& OutError);
	static bool DecodeVoxelProxy(
		TConstArrayView<uint8> InBytes,
		FVoxelVoxelProxyData& OutData,
		FString& OutError);
	static bool EncodeSurface(
		const FVoxelSurfaceTileData& InData,
		TArray<uint8>& OutBytes,
		FString& OutError);
	static bool DecodeSurface(
		TConstArrayView<uint8> InBytes,
		FVoxelSurfaceTileData& OutData,
		FString& OutError);
	static bool EncodeMacro(
		const FVoxelMacroTileData& InData,
		TArray<uint8>& OutBytes,
		FString& OutError);
	static bool DecodeMacro(
		TConstArrayView<uint8> InBytes,
		FVoxelMacroTileData& OutData,
		FString& OutError);
};
