
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
	virtual void Initialize(UVoxelModule* InModule);

	virtual void Generate(UVoxelChunk* InChunk);

protected:
	UPROPERTY(BlueprintReadOnly)
	UVoxelModule* Module;

public:
	bool IsGenerationEnabled() const { return bGenerationEnabled; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bGenerationEnabled;
};
