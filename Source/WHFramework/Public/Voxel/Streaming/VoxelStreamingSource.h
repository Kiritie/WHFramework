#pragma once

#include "CoreMinimal.h"

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
	SimulationAnchor
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

struct WHFRAMEWORK_API FVoxelStreamingSource
{
	FGuid Id;
	EVoxelStreamingSourcePurpose Purpose = EVoxelStreamingSourcePurpose::Observer;
	bool bAffectsGlobalReadiness = false;
	bool bRetainGenerationCache = true;
	int32 RetentionRadiusCells = 0;
	FIntVector Center = FIntVector::ZeroValue;
	FVector Direction = FVector::ForwardVector;
	float VerticalFovDegrees = 90.0f;
	int32 ViewportHeightPixels = 1080;
	int32 ExactRadius = 16;
	int32 CollisionRadius = 8;
	int32 SimulationRadius = 6;
	int32 VerticalExactRadius = 8;
	int32 MovementCriticalCollisionRadius = 0;
	EVoxelStreamingRenderMode RenderMode = EVoxelStreamingRenderMode::Full;
	bool bCollision = true;
	bool bSimulation = true;
};
