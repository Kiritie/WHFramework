#include "Voxel/Runtime/VoxelRuntimeDefinition.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
const FVoxelRuntimeFaceRef& FVoxelRuntimeDefinition::Face(uint16 S,uint8 F)const
{check(F<6);return Shape==EVoxelShapeKind::Door&&(S&8)?UpperFaces[F]:Faces[F];}

const FVoxelResolvedShape* FVoxelRuntimeDefinition::FindShape(const FVoxelShapeRegistry& InShapes, uint16 InState) const
{
    if (!StateGeometry.IsEmpty()) return StateGeometry.Find(uint16(InState & ~VoxelState::ObjectLockedMask));
    return InShapes.Find(Shape, InState);
}

const FVoxelResolvedShape& FVoxelRuntimeDefinition::GetShape(const FVoxelShapeRegistry& InShapes, uint16 InState) const
{
    const FVoxelResolvedShape* Result = FindShape(InShapes, InState);
    check(Result);
    return *Result;
}
