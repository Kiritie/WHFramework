#include "Voxel/VoxelModuleTypes.h"
bool FVoxelItem::IsValid() const
{
	return VoxelAssetID.IsValid() && State >= 0 && State <= 65535 && Count > 0 && Count <= 100000;
}
