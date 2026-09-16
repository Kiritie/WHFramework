#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
enum class EVoxelTaskKind : uint8
{
	Generate,
	LoadDelta,
	RebuildOverlay,
	Mesh,
	Collision,
	SaveEncode,
	LoadScene,
	EncodeNetwork,
	DecodeNetwork
};
struct WHFRAMEWORK_API FVoxelTaskStamp
{
	uint64 WorldEpoch = 0;
	uint64 GenerationToken = 0;
	uint64 Revision = 0;
	uint64 GeometryVersion = 0;
	FVoxelSectionKey Key;
	bool operator==(const FVoxelTaskStamp& B) const
	{
		return WorldEpoch == B.WorldEpoch && GenerationToken == B.GenerationToken && Revision == B.Revision && GeometryVersion == B.GeometryVersion &&
		       Key == B.Key;
	}
};
