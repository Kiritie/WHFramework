#pragma once

#include "Voxel/VoxelModuleTypes.h"
#include "Input/InputModuleTypes.h"

#include "VoxelAgentInterface.generated.h"

class AVoxelEntityPreview;

UINTERFACE()
class WHFRAMEWORK_API UVoxelAgentInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IVoxelAgentInterface
{
	GENERATED_BODY()

public:
	IVoxelAgentInterface();

public:
	virtual bool OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

	virtual bool OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

	virtual bool OnInteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

public:
	virtual FVector GetVoxelAgentLocation() const = 0;

	virtual FPrimaryAssetId GetGenerateVoxelID() const { return FPrimaryAssetId(); }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) { }

	virtual float GetDestroyVoxelRate() const { return 1.f; }

	virtual void SetDestroyVoxelRate(float InRate) { }

protected:
	bool bCanGenerateVoxel;
	FVoxelItem GenerateVoxelItem;
	TObjectPtr<AVoxelEntityPreview> GeneratePreviewVoxel;
	FVoxelItem DestroyVoxelItem;
};
