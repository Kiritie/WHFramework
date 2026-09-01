#include "Voxel/Prefabs/Data/VoxelPrefabData.h"

UVoxelPrefabData::UVoxelPrefabData()
{
	Type = FName("VoxelPrefab");
	
	DisplayName = FText::GetEmpty();
	VoxelDatas = TEXT("");
	VoxelSize = FVector::ZeroVector;
	CenterOffset = FVector::ZeroVector;
}

void UVoxelPrefabData::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void UVoxelPrefabData::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

FBox UVoxelPrefabData::GetVoxelBounds() const
{
	const FVector HalfSize = VoxelSize * 0.5f;
	return FBox(
		FVector(CenterOffset.X - HalfSize.X, CenterOffset.Y - HalfSize.Y, CenterOffset.Z),
		FVector(CenterOffset.X + HalfSize.X, CenterOffset.Y + HalfSize.Y, CenterOffset.Z + VoxelSize.Z));
}
