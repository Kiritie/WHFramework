#include "Voxel/Voxels/Data/VoxelTorchData.h"
UVoxelTorchData::UVoxelTorchData()
{
	Shape = EVoxelShapeKind::Torch;
	RenderGroup = EVoxelRenderGroup::Emissive;
	bSolid = false;
	bOccludes = false;
}
