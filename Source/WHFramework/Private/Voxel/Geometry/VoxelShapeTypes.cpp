#include "Voxel/Geometry/VoxelShapeTypes.h"
bool FVoxelResolvedShape::Covers(uint8 F, int32 U, int32 V) const
{
	if (F >= 6 || U < 0 || U >= 16 || V < 0 || V >= 16)
		return false;
	int32 I = U + V * 16;
	return (Coverage[F][I / 64] & (uint64(1) << (I % 64))) != 0;
}
