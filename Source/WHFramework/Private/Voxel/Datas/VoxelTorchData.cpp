#include "Voxel/Datas/VoxelTorchData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelTorch.h"
#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorchData::UVoxelTorchData()
{
	VoxelType = EVoxelType::Torch;
	VoxelClass = UVoxelTorch::StaticClass();
	AuxiliaryClass = AVoxelTorchAuxiliary::StaticClass();

	Transparency = EVoxelTransparency::Transparent;

	MeshScale = FVector(0.1f, 0.1f, 0.5f);
	MeshOffset = FVector(0.f, 0.f, -0.25f);

	EffectAsset = nullptr;
	EffectScale = FVector::OneVector;
	EffectOffset = FVector::ZeroVector;
}
