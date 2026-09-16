#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

UVoxelPrefabData::UVoxelPrefabData()
{
	Type = TEXT("VoxelPrefab");
	DisplayName = FText::GetEmpty();
}

FBox UVoxelPrefabData::GetVoxelBounds() const
{
	FBox Bounds(ForceInit);
	for (const FVoxelPrefabCell& Cell : Data.Cells)
	{
		const FVector Min(Cell.Offset);
		Bounds += Min;
		Bounds += Min + FVector(1.0);
	}
	return Bounds;
}

bool UVoxelPrefabData::Validate(const FVoxelRegistrySnapshot& Registry, FString& Error) const
{
	return ValidateCells(Data, Registry, Error);
}

bool UVoxelPrefabData::ValidateCells(const FVoxelPrefabSaveData& Value, const FVoxelRegistrySnapshot& Registry, FString& Error)
{
	Error.Reset();
	if (Value.Cells.Num() > VoxelPrefab::MaxCellCount)
	{
		Error = FString::Printf(TEXT("Prefab exceeds %d cells"), VoxelPrefab::MaxCellCount);
		return false;
	}
	TMap<FIntVector, FVoxelBlockState> Blocks;
	TSet<FVoxelSectionKey> Sections;
	for (const FVoxelPrefabCell& Cell : Value.Cells)
	{
		FVoxelBlockState State;
		if (!VoxelCoord::IsValid(Cell.Offset) || Cell.Item.Count != 1 || !FVoxelItemBridge::ToBlock(Registry, Cell.Item, State) || State.IsAir() ||
		    Blocks.Contains(Cell.Offset))
		{
			Error = FString::Printf(TEXT("Invalid, duplicate, or non-unit prefab cell at %s"), *Cell.Offset.ToString());
			return false;
		}
		Blocks.Add(Cell.Offset, State);
		Sections.Add(VoxelCoord::Section(Cell.Offset));
	}
	if (Sections.Num() > VoxelPrefab::MaxSectionCount)
	{
		Error = FString::Printf(TEXT("Prefab spans more than %d local sections"), VoxelPrefab::MaxSectionCount);
		return false;
	}
	for (const auto& Pair : Blocks)
	{
		const FVoxelRuntimeDefinition* Definition = Registry.Find(Pair.Value.TypeId);
		if (!Definition)
		{
			Error = TEXT("Prefab references an unregistered block");
			return false;
		}
		if (Definition->Shape != EVoxelShapeKind::Door)
		{
			continue;
		}
		const bool bUpper = (Pair.Value.State & VoxelState::HalfMask) != 0;
		const FIntVector OtherPosition = Pair.Key + FIntVector(0, 0, bUpper ? -1 : 1);
		const FVoxelBlockState* Other = Blocks.Find(OtherPosition);
		const uint16 OtherState = Pair.Value.State ^ VoxelState::HalfMask;
		if (!Other || Other->TypeId != Pair.Value.TypeId || Other->State != OtherState)
		{
			Error = FString::Printf(TEXT("Door halves are missing or inconsistent at %s"), *Pair.Key.ToString());
			return false;
		}
	}
	// Empty means an explicitly empty asset/preview. World placement rejects an empty edit.
	return true;
}
