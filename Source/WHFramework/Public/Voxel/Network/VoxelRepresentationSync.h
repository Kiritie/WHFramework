#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelProxyPageBuild.h"
#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelTerrainResources.h"

class UVoxelModule;
struct FVoxelGeometryWireHeader;

struct WHFRAMEWORK_API FVoxelRepresentationBuildInput
{
	FVoxelRepresentationRequest Request;
	FVoxelGenerationSettings Settings;
	FVoxelLodTopologySettings Lod;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	FVoxelFineOverlayCapture OverlayCapture;
	uint64 Revision = 0;
};

class WHFRAMEWORK_API FVoxelRepresentationPageBuild
{
public:
	FVoxelRepresentationPageBuild(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Initialize(FVoxelRepresentationBuildInput&& InInput, FString& OutError);
	bool Advance(FVoxelBinaryBuffer& OutBytes, bool& bOutComplete, FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);
	uint64 InputBytes() const;
	uint64 RawCapacityBytes() const;
	uint64 MaximumWireBytes() const;
	bool NeedsGeneration() const { return !bGenerated; }
	const FVoxelFineOverlayReadResult& GetOverlayReadResult() const;

private:
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelRepresentationBuildInput Input;
	FVoxelFineOverlayRead OverlayReader;
	FVoxelFineOverlayReadResult OverlayReadResult;
	FVoxelProxyPageBuild Proxy;
	FVoxelHeightfieldPageBuild Heightfield;
	FVoxelVoxelProxyData ProxyData;
	FVoxelSurfaceTileData SurfaceData;
	FVoxelMacroTileData MacroData;
	bool bGenerated = false;
	bool bEncodedTaken = false;
};

class WHFRAMEWORK_API FVoxelRepresentationSync
{
public:
	static bool PrepareServerBuild(
		UVoxelModule& InModule,
		const FVoxelRepresentationRequest& InRequest,
		FVoxelRepresentationBuildInput& OutInput,
		FString& OutError, const FVoxelGenerationConsumerKey* InConsumer = nullptr);
	static bool EncodeVoxelProxy(
		const FVoxelVoxelProxyData& InData,
		FVoxelBinaryBuffer& OutBytes,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	static bool DecodeVoxelProxy(
		TConstArrayView<uint8> InBytes,
		FVoxelVoxelProxyData& OutData,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGeometryWireHeader* InExpected = nullptr);
	static bool EncodeSurface(
		const FVoxelSurfaceTileData& InData,
		FVoxelBinaryBuffer& OutBytes,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	static bool DecodeSurface(
		TConstArrayView<uint8> InBytes,
		FVoxelSurfaceTileData& OutData,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGeometryWireHeader* InExpected = nullptr);
	static bool EncodeMacro(
		const FVoxelMacroTileData& InData,
		FVoxelBinaryBuffer& OutBytes,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	static bool DecodeMacro(
		TConstArrayView<uint8> InBytes,
		FVoxelMacroTileData& OutData,
		FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGeometryWireHeader* InExpected = nullptr);
};
