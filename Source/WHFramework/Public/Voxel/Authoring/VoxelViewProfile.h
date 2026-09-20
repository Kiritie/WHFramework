#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "VoxelViewProfile.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelViewProfile : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelViewProfile();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0"))
	int32 WarmupDataRadiusCentimeters = 600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0"))
	int32 WarmupCollisionRadiusCentimeters = 600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 FineRadiusCentimeters = 8000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 VoxelProxyRadiusCentimeters = 80000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 SurfaceRadiusCentimeters = 800000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 MacroRadiusCentimeters = 3000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0.1"))
	float TargetScreenErrorPixels = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumVoxelProxyLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumSurfaceLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumMacroLevel = 4;
};
