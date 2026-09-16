#include "Voxel/Rendering/VoxelRenderTypes.h"
const FVoxelFaceTexture& FVoxelFaceMaterialSet::Get(uint8 F) const
{
	check(F < 6);
	switch (F)
	{
		case 0:
			return PositiveX;
		case 1:
			return NegativeX;
		case 2:
			return PositiveY;
		case 3:
			return NegativeY;
		case 4:
			return Top;
		default:
			return Bottom;
	}
}
