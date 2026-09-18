#include "Voxel/Runtime/VoxelRuntimeDefinition.h"
const FVoxelRuntimeFaceRef& FVoxelRuntimeDefinition::Face(uint16 S,uint8 F)const
{check(F<6);return Shape==EVoxelShapeKind::Door&&(S&8)?UpperFaces[F]:Faces[F];}
