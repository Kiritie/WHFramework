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

	friend class UVoxel;

public:
	IVoxelAgentInterface();

public:
	virtual bool InteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

	virtual void UnInteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent);

protected:
	virtual bool OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

	virtual bool OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult);

public:
	virtual FVector GetVoxelAgentLocation() const { return FVector::ZeroVector; }

	virtual FPrimaryAssetId GetGenerateVoxelID() const { return FPrimaryAssetId(); }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) { }

	virtual EVoxelGenerateToolType GetGenerateToolType() const { return EVoxelGenerateToolType::None; }

	virtual void SetGenerateToolType(EVoxelGenerateToolType InGenerateToolType) { }

	virtual float GetDestroyVoxelRate() const { return 1.f; }

protected:
	bool bCanGenerateVoxel;
	FVoxelItem GenerateVoxelItem;
	TObjectPtr<AVoxelEntityPreview> GeneratePreviewVoxel;
	
	FVoxelItem DestroyVoxelItem;
};
