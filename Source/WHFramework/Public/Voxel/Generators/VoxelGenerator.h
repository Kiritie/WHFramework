
#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Thread/IThreadSafeInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelGenerator.generated.h"

class UVoxelModule;
class UVoxelChunk;

/** 体素生成器 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UVoxelGenerator : public UWHObject, public IThreadSafeInterface
{
	GENERATED_BODY()
	
public:
	UVoxelGenerator();
	
public:
	virtual void Initialize(UVoxelModule* InModule, int32 InStage);

	virtual void PrepareBatch(const TArray<FIndex>& InChunkIndices);

	virtual void Generate(UVoxelChunk* InChunk);

	virtual void CompleteBatch(bool bCancelled);

protected:
	UPROPERTY(BlueprintReadOnly)
	UVoxelModule* Module;

protected:
	UPROPERTY(BlueprintReadOnly)
	int32 Stage;

public:
	int32 GetStage() const { return Stage; }
};
