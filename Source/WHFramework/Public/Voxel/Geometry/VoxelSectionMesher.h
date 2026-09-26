#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
struct FVoxelBoundaryTransitionContext;
struct WHFRAMEWORK_API FVoxelMeshBuffers
{
    TArray<FVector> Vertices,Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV0,UV1,UV2;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    uint64 Bytes()const;
    bool Validate()const;
};
struct WHFRAMEWORK_API FVoxelRenderBatch
{
    EVoxelRenderGroup Group=EVoxelRenderGroup::Opaque;uint16 Bank=0;FVoxelMeshBuffers Mesh;
};
struct WHFRAMEWORK_API FVoxelSectionMeshResult
{
    FVoxelTaskStamp Stamp;
    TArray<FVoxelRenderBatch> Batches;
    uint64 Bytes()const;
};
class WHFRAMEWORK_API FVoxelSectionMesher
{
public:
    static bool Build(const FVoxelSectionSnapshot& Snapshot,const FVoxelRegistrySnapshot& Registry,
        const FVoxelShapeRegistry& Shapes,FVoxelSectionMeshResult& Out,const TAtomic<bool>* Cancel=nullptr,
        double InTextureRepeatsPerCell=1.0,
        const FVoxelBoundaryTransitionContext* InTransition=nullptr);
};
