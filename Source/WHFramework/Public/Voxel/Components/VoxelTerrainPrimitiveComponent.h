#pragma once

#include "Components/MeshComponent.h"
#include "Voxel/Task/VoxelTerrainResources.h"
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
	virtual ~UVoxelTerrainPrimitiveComponent() override;
	virtual int32 GetNumMaterials() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(
		const FTransform& InLocalToWorld) const override;
	void InitializeTerrain(
		TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> InState);
	int32 RetainMaterial(UMaterialInterface* InMaterial, EVoxelTerrainResourceAdmission* OutAdmission = nullptr);
	UVoxelTerrainPrimitiveComponent* DetachMaterialKeeper();
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetNativeMaterialLease() const { return MaterialLease; }

	TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> TerrainState;

private:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MaterialLease;
};
