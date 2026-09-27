#pragma once

#include "CoreMinimal.h"
#include "VoxelStreamingSource.generated.h"

enum class EVoxelStreamingRenderMode : uint8
{
	None = 0,
	FineOnly,
	Full
};

enum class EVoxelStreamingSourcePurpose : uint8
{
	None = 0,
	Observer,
	InitialSpawn,
	TravelPrewarm,
	RespawnPrewarm,
	SimulationAnchor,
	DebugPOI
};

struct WHFRAMEWORK_API FVoxelStreamingReadiness
{
	bool bAdmitted = false;
	int32 RequiredDataSections = 0;
	int32 ReadyDataSections = 0;
	int32 RequiredCollisionSections = 0;
	int32 ReadyCollisionSections = 0;

	bool IsDataReady() const
	{
		return bAdmitted && RequiredDataSections > 0 && ReadyDataSections == RequiredDataSections;
	}

	bool IsCollisionReady() const
	{
		return IsDataReady() && RequiredCollisionSections > 0 && ReadyCollisionSections == RequiredCollisionSections;
	}
};

// Each source owns its view ranges; components and temporary sources can set them independently.
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStreamingSourceView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 WarmupDataRadiusCells = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 WarmupCollisionRadiusCells = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 FineRadiusCells = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float StreamingReplanFineRadiusFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 FineVerticalRadiusCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 FinePreloadCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 VoxelProxyRadiusCells = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 SurfaceRadiusCells = 3200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|View", meta = (ClampMin = "0.1"))
	float TargetScreenErrorPixels = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|View", meta = (ClampMin = "1"))
	uint8 MaximumVoxelProxyLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|View", meta = (ClampMin = "1"))
	uint8 MaximumSurfaceLevel = 4;

};

struct WHFRAMEWORK_API FVoxelStreamingSource
{
	FGuid Id;
	EVoxelStreamingSourcePurpose Purpose = EVoxelStreamingSourcePurpose::Observer;
	int32 Priority = 0;
	bool bLocalView = false;
	uint64 RegistrationOrder = 0;
	int32 SchedulingPriority = INDEX_NONE;
	bool bRefineView = true;
	bool bWorldView = true;

	int32 GetSchedulingPriority() const
	{
		return SchedulingPriority == INDEX_NONE ? Priority : SchedulingPriority;
	}
	bool bAffectsGlobalReadiness = false;
	bool bRetainGenerationCache = true;
	int32 RetentionRadiusCells = 0;
	FIntVector Center = FIntVector::ZeroValue;
	FVector Direction = FVector::ForwardVector;
	float VerticalFovDegrees = 90.0f;
	int32 ViewportHeightPixels = 1080;
	int32 ExactRadius = 16;
	bool bInheritWorldView = true;
	FVoxelStreamingSourceView View;
	int32 CollisionRadius = 8;
	int32 SimulationRadius = 6;
	int32 VerticalExactRadius = 8;
	int32 MovementCriticalCollisionRadius = 0;
	EVoxelStreamingRenderMode RenderMode = EVoxelStreamingRenderMode::Full;
	bool bCollision = true;
	bool bSimulation = true;
};
