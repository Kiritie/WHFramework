#include "Voxel/Datas/VoxelDoorData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

UVoxelDoorData::UVoxelDoorData()
{
	MaxCount = 10;
	
	VoxelClass = UVoxelDoor::StaticClass();
	AuxiliaryClass = AVoxelDoorAuxiliary::StaticClass();

	Transparency = EVoxelTransparency::SemiTransparent;

	MeshType = EVoxelMeshType::Custom;
	
	MeshVertices = TArray<FVector>();
	MeshVertices.Add(FVector(-0.5f, -0.5f, -0.5f));
	MeshVertices.Add(FVector(-0.5f, -0.5f, 0.5f));
	MeshVertices.Add(FVector(0.5f, 0.5f, 0.5f));
	MeshVertices.Add(FVector(0.5f, 0.5f, -0.5f));
	MeshVertices.Add(FVector(0.5f, -0.5f, -0.5f));
	MeshVertices.Add(FVector(0.5f, -0.5f, 0.5f));
	MeshVertices.Add(FVector(-0.5f, 0.5f, 0.5f));
	MeshVertices.Add(FVector(-0.5f, 0.5f, -0.5f));

	MeshNormals = TArray<FVector>();
	MeshNormals.Add(FVector(1, -1, 0));
	MeshNormals.Add(FVector(-1, -1, 0));

	OpenSound = nullptr;
	CloseSound = nullptr;
}

bool UVoxelDoorData::GetMeshDatas(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation) const
{
	if(MeshType == EVoxelMeshType::Custom)
	{
		const FVector range = GetRange(InRotation);

		const bool bOpened = InVoxelItem.GetVoxel<UVoxelDoor>().IsOpened();

		for(int32 i = (!bOpened ? 0 : MeshVertices.Num() / 2 - 1); i < (!bOpened ? MeshVertices.Num() / 2 : MeshVertices.Num()); i++)
		{
			if(MeshVertices.IsValidIndex(i))
			{
				OutMeshVertices.Add(MeshVertices[i] * range); 
			}
		}

		for(auto Iter : MeshNormals)
		{
			OutMeshNormals.Add(Iter.GetSafeNormal());
		}
		return true;
	}
	return false;
}
