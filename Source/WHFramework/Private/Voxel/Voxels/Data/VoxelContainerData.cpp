#include "Voxel/Voxels/Data/VoxelContainerData.h"

#include "Voxel/Save/InventoryVoxelEntityCodec.h"

UVoxelContainerData::UVoxelContainerData()
{
    EntityKind = FInventoryVoxelEntityCodec::Kind;
    EntityVariant = 0;
}
