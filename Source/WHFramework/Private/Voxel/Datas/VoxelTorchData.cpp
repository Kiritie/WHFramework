#include "Voxel/Datas/VoxelTorchData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorchData::UVoxelTorchData()
{
	VoxelClass = UVoxelTorchData::StaticClass();
	AuxiliaryClass = AVoxelTorchAuxiliary::StaticClass();

	Transparency = EVoxelTransparency::Transparent;

	MeshScale = FVector(0.2f, 0.2f, 0.8f);
	MeshOffset = FVector(0.f, 0.f, -0.2f);

	Effect = nullptr;
	EffectScale = FVector::OneVector;
}
