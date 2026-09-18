#pragma once
#include "ProceduralMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "VoxelMeshComponent.generated.h"
UCLASS()
class WHFRAMEWORK_API UVoxelMeshComponent:public UProceduralMeshComponent
{
    GENERATED_BODY()
public:
    explicit UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer);
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    bool Apply(const FVoxelMeshBuffers& Mesh,double BlockSize,UMaterialInterface* Material);
};
