#include "Voxel/Prefabs/Data/VoxelPrefabData.h"

UVoxelPrefabData::UVoxelPrefabData()
{
	Type = FName("VoxelPrefab");
	
	DisplayName = FText::GetEmpty();
	VoxelDatas = TEXT("");
	VoxelSize = FVector::ZeroVector;
}

void UVoxelPrefabData::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void UVoxelPrefabData::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}
