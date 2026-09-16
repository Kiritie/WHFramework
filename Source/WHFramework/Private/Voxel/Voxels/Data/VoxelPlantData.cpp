#include "Voxel/Voxels/Data/VoxelPlantData.h"
UVoxelPlantData::UVoxelPlantData()
{
	Shape = EVoxelShapeKind::CrossPlant;
	RenderGroup = EVoxelRenderGroup::Foliage;
	bSolid = false;
	bOccludes = false;
	bReplaceable = true;
}
