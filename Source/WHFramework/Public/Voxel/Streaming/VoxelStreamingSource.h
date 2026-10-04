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
	if (EnumHasAnyFlags(InCapabilities, EVoxelStreamingCapability::Simulation))
	{
		InCapabilities |= EVoxelStreamingCapability::Collision;
	}
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStreamingSourceView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "16", UIMin = "16"))
	int32 InitialChunkSideCells = 16;

	// Fine Page F0 的最小完整分辨率区域。
	// 它不改变 Fine 数据范围，只决定靠近 Source 的 Section 不能被合并到 F1+ Page。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 FineLevel0RadiusCells = 32;

	// Fine Representation 的水平外半径。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 FineRadiusCells = 80;

	// Fine Representation 的 Z 半高；不是三维球半径。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 FineVerticalHalfExtentCells = 64;

	// 移动预测时间。只用于前向 Critical / Prefetch，不扩大 Fine 显示域。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0.0"))
	float FinePredictionSeconds = 0.75f;

	// VoxelProxy 的水平外半径。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|View",
		meta = (ClampMin = "0"))
	int32 VoxelProxyRadiusCells = 1024;

	// VoxelProxy 独立 Z 半高；禁止继续借 FineVerticalHalfExtentCells。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|View",
		meta = (ClampMin = "0"))
	int32 VoxelProxyVerticalHalfExtentCells = 128;

	// Surface 外半径。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|View",
		meta = (ClampMin = "0"))
	int32 SurfaceRadiusCells = 3200;

	// Macro 外半径。原先在 ViewProfile 外层，本轮移入 SourceView，使四层 Domain 统一。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|View",
		meta = (ClampMin = "0"))
	int32 MacroRadiusCells = 16000;
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
