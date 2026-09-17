#pragma once
#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/Kernel/VoxelViewPlan.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
struct WHFRAMEWORK_API FVoxelLodMeshInput
{
    std::shared_ptr<const VoxelView::Grid> Current;
    std::vector<std::shared_ptr<const VoxelView::Grid>> Neighbors;
    uint64 CoverageSerial=0;
};
struct WHFRAMEWORK_API FVoxelLodBuiltMesh
{
    VoxelView::Key Key;
    uint64 CoverageSerial=0;
    FVoxelSectionMeshResult Mesh;
    uint64 Bytes() const{return Mesh.Bytes();}
};
class WHFRAMEWORK_API FVoxelLodMesher
{
public:
    static bool Build(const FVoxelLodMeshInput& Input,const FVoxelGenerationPipeline& Generator,
                      const FVoxelRegistrySnapshot& Registry,const FVoxelShapeRegistry& Shapes,
                      FVoxelLodBuiltMesh& Out,FString& Error,const std::atomic_bool* Cancel=nullptr);
};
