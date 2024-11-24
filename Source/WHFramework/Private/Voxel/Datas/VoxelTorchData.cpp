#include "Voxel/Datas/VoxelTorchData.h"

#include "Voxel/Voxels/VoxelTorch.h"
#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorchData::UVoxelTorchData()
{
	VoxelType = EVoxelType::Torch;
	VoxelClass = UVoxelTorch::StaticClass();
	AuxiliaryClass = AVoxelTorchAuxiliary::StaticClass();

	Nature = EVoxelNature::SmallSemiSolid;

	MeshDatas.SetNum(2);
	
	MeshDatas[0].MeshScale = FVector(0.1f, 0.1f, 0.5f);
	MeshDatas[0].MeshOffset = FVector(0.f, 0.f, -0.25f);
	
	MeshDatas[1].MeshScale = FVector(0.1f, 0.1f, 0.5f);
	MeshDatas[1].MeshOffset = FVector(0.f, 0.f, -0.25f);

	EffectAsset = nullptr;
	EffectScale = FVector::OneVector;
	EffectOffset = FVector::ZeroVector;
}
