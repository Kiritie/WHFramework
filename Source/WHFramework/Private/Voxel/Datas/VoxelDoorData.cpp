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

	MeshScale = FVector(0.1f, 1.f, 1.f);
	MeshOffset = FVector(-0.45f, 0.f, 0.f);
	
	OpenedMeshScale = FVector(1.f, 0.1f, 1.f);
	OpenedMeshOffset = FVector(0.f, 0.45f, 0.f);
	OpenedMeshUVDatas = TArray<FVoxelMeshUVData>();
}

void UVoxelDoorData::GetMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const
{
	if(InVoxelItem.Owner && InVoxelItem.GetVoxel<UVoxelDoor>().IsOpened())
	{
		OutMeshScale = OpenedMeshScale;
		OutMeshOffset = OpenedMeshOffset;
	}
	else
	{
		Super::GetMeshData(InVoxelItem, OutMeshScale, OutMeshOffset);
	}
}

void UVoxelDoorData::GetUVData(const FVoxelItem& InVoxelItem, int32 InFaceIndex, FVector2D InUVSize, FVector2D& OutUVCorner, FVector2D& OutUVSpan) const
{
	if(InVoxelItem.Owner && InVoxelItem.GetVoxel<UVoxelDoor>().IsOpened())
	{
		if(!OpenedMeshUVDatas.IsValidIndex(InFaceIndex)) return;
		
		const FVoxelMeshUVData& UVData = OpenedMeshUVDatas[InFaceIndex];
		OutUVCorner = FVector2D(UVData.UVCorner.X * InUVSize.X, (1.f / InUVSize.Y - UVData.UVCorner.Y - UVData.UVSpan.Y) * InUVSize.Y);
		OutUVSpan = FVector2D(UVData.UVSpan.X * InUVSize.X, UVData.UVSpan.Y * InUVSize.Y);
	}
	else
	{
		Super::GetUVData(InVoxelItem, InFaceIndex, InUVSize, OutUVCorner, OutUVSpan);
	}
}
