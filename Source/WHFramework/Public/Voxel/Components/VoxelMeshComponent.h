#pragma once

#include "ProceduralMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "VoxelMeshComponent.generated.h"

UCLASS()
class WHFRAMEWORK_API UVoxelMeshComponent :
	public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	explicit UVoxelMeshComponent(
		const FObjectInitializer& InObjectInitializer);

	virtual FBoxSphereBounds CalcBounds(
		const FTransform& InLocalToWorld) const override;

	bool Apply(
		const FVoxelMeshBuffers& InMesh,
		double InBlockSize,
		UMaterialInterface* InMaterial);
};
