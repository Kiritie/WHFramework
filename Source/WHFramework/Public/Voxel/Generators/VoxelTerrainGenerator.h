
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelTerrainGenerator.generated.h"

/** 体素地形生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTerrainGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelTerrainGenerator();
	
public:
	virtual void Generate(UVoxelChunk* InChunk) override;

public:
	virtual EVoxelType CalculateVoxelType(UVoxelChunk* InChunk, FIndex InIndex) const;

	virtual EVoxelType GetBiomeVoxelType(EVoxelBiomeType InBiomeType, bool bUnderGround) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UnderDepth;
};
