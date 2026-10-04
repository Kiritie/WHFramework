#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

struct WHFRAMEWORK_API FVoxelPagePresentation
{
	FVoxelTerrainBuildStamp Stamp;
	bool bCommitted = false;
	bool bVisible = false;
	bool bEmpty = true;
};

struct WHFRAMEWORK_API FVoxelPagePublicationInput
{
	FVoxelPagePresentation Desired;
	FVoxelPagePresentation Committed;
	bool bCompleteFamily = false;
	bool bAllMeshesReady = false;
	bool bNewDependenciesReady = false;
	bool bOldOwnershipCovered = false;
	bool bLayoutSame = false;
	bool bActorPresentationReady = false;
	bool bPendingPublication = false;
};

enum class EVoxelPagePublicationDecision : uint8
{
	None = 0,
	KeepPrevious,
	NoChange,
	Publish,
	WaitForPublication
};

namespace VoxelPagePresentation
{
	WHFRAMEWORK_API EVoxelPagePublicationDecision Decide(const FVoxelPagePublicationInput& InInput);
}
