#include "Voxel/Datas/VoxelTreeData.h"

UVoxelTreeData::UVoxelTreeData()
{
	Transparency = EVoxelTransparency::Solid;

	bTreeRoot = true;
	TreeHeight = FVector2D(4.f, 5.f);
	LeavesHeight = FVector2D(2.f);
	LeavesWidth = FVector2D(3.f, 5.f);
}
