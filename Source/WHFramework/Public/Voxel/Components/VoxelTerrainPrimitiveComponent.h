#pragma once

#include "Components/MeshComponent.h"
#include "VoxelTerrainPrimitiveComponent.generated.h"

struct FVoxelTerrainRendererState;

UCLASS(Transient)
class WHFRAMEWORK_API UVoxelTerrainPrimitiveComponent :
	public UMeshComponent
{
	GENERATED_BODY()

public:
	explicit UVoxelTerrainPrimitiveComponent(
		const FObjectInitializer& InObjectInitializer);
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(
		const FTransform& InLocalToWorld) const override;
	void InitializeTerrain(
		TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> InState);
	int32 RetainMaterial(UMaterialInterface* InMaterial);

	TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> TerrainState;
};
