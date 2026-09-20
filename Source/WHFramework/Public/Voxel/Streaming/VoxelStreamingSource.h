#pragma once

#include "CoreMinimal.h"

enum class EVoxelStreamingRenderMode : uint8
{
	None = 0,
	FineOnly,
	Full
};

struct WHFRAMEWORK_API FVoxelStreamingSource
{
	FGuid Id;
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
