#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/WorldObject/VoxelWorldObjectDefinition.h"

class FVoxelWorldRuntime;
struct FVoxelRegistrySnapshot;

struct WHFRAMEWORK_API FVoxelWorldObjectInstance
{
	FName DefinitionId;
	FIntVector Anchor = FIntVector::ZeroValue;
	uint8 Yaw = 0;
	FVoxelBlockState State;

	FGuid StableId() const;
};

struct WHFRAMEWORK_API FVoxelWorldObjectDefinitionRuntime
{
	FName Id;
	FName UseAction;
	TArray<FVoxelWorldObjectPart> Parts;
	TArray<uint16> Types;
	uint16 ToggleMask = 0;
	TWeakObjectPtr<UVoxelWorldObjectDefinition> Source;
};

class WHFRAMEWORK_API FVoxelWorldObjectRegistry
{
public:
	bool Build(const FVoxelRegistrySnapshot& InBlocks, const TArray<UVoxelWorldObjectDefinition*>& InDefinitions, FString& OutError);
	void Reset();
	double GetMaxMaterializationRadius() const { return MaxMaterializationRadius; }
	const FVoxelWorldObjectDefinitionRuntime* Find(FName InId) const;
	const FVoxelWorldObjectDefinitionRuntime* Find(uint16 InType) const;
	bool Resolve(const FVoxelWorldRuntime& InWorld, const FIntVector& InCell, FVoxelWorldObjectInstance& OutObject, FString& OutError) const;
	static FIntVector Rotate(const FIntVector& InOffset, uint8 InYaw);
	static FVoxelBlockState PartState(const FVoxelWorldObjectDefinitionRuntime& InDefinition, int32 InPart, uint8 InYaw, uint16 InFlags = 0);

private:
	TMap<FName, FVoxelWorldObjectDefinitionRuntime> Definitions;
	TMap<uint16, FName> TypeDefinitions;
	double MaxMaterializationRadius = 0.;
};
