#pragma once

#include "CoreMinimal.h"
#include "VoxelStreamingSource.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class EVoxelStreamingCapability : uint8
{
	None = 0,
	Data = 1 << 0,
	Collision = 1 << 1,
	Simulation = 1 << 2,
	FineVisual = 1 << 3,
	WorldVisual = 1 << 4,
	LocalRefinement = 1 << 5,
	Readiness = 1 << 6,
	RetainGenerationCache = 1 << 7
};
ENUM_CLASS_FLAGS(EVoxelStreamingCapability)

FORCEINLINE EVoxelStreamingCapability NormalizeVoxelStreamingCapabilities(
	EVoxelStreamingCapability InCapabilities)
{
	if (EnumHasAnyFlags(InCapabilities,
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::Simulation |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::WorldVisual |
		EVoxelStreamingCapability::LocalRefinement |
		EVoxelStreamingCapability::Readiness))
	{
		InCapabilities |= EVoxelStreamingCapability::Data;
	}
	if (EnumHasAnyFlags(InCapabilities,
		EVoxelStreamingCapability::WorldVisual |
		EVoxelStreamingCapability::LocalRefinement))
	{
		InCapabilities |= EVoxelStreamingCapability::FineVisual;
	}
	return InCapabilities;
}

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "1"))
	int32 FineReplanCells = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "1"))
	int32 ProxyReplanCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "1"))
	int32 SurfaceReplanCells = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "1"))
	int32 MacroReplanCells = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0.0"))
	float FinePredictionSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ClampMin = "0"))
	int32 MovementCriticalFineRadiusCells = 48;

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
	EVoxelStreamingCapability Capabilities = EVoxelStreamingCapability::None;

	int32 GetSchedulingPriority() const
	{
		return SchedulingPriority == INDEX_NONE ? Priority : SchedulingPriority;
	}
	int32 RetentionRadiusCells = 0;
	FIntVector Center = FIntVector::ZeroValue;
	FVector Direction = FVector::ForwardVector;
	FVector VelocityCellsPerSecond = FVector::ZeroVector;
	float VerticalFovDegrees = 90.0f;
	int32 ViewportHeightPixels = 1080;
	int32 ExactRadius = 16;
	bool bInheritWorldView = true;
	FVoxelStreamingSourceView View;
	int32 CollisionRadius = 8;
	int32 SimulationRadius = 6;
	int32 VerticalExactRadius = 8;
	int32 MovementCriticalCollisionRadius = 0;
	bool Has(const EVoxelStreamingCapability InCapability) const
	{
		return EnumHasAnyFlags(Capabilities, InCapability);
	}

	void Normalize()
	{
		Capabilities = NormalizeVoxelStreamingCapabilities(Capabilities);
	}
};
