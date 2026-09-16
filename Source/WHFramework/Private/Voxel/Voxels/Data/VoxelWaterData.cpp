#include "Voxel/Voxels/Data/VoxelWaterData.h"
UVoxelWaterData::UVoxelWaterData()
{
	Shape = EVoxelShapeKind::Fluid;
	RenderGroup = EVoxelRenderGroup::Water;
	bSolid = false;
	bOccludes = false;
	bReplaceable = true;
	bBreakable = false;
	DropCount = 0;
}
