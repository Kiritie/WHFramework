#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

struct WHFRAMEWORK_API FVoxelGeometryWireHeader
{
	FVoxelTerrainGeometry Geometry;
	uint64 RequestId = 0;
	uint64 ContentRevision = 0;
};

namespace VoxelGeometryWire
{
	inline constexpr uint16 Version = 6;
	inline constexpr int32 HeaderBytes = 64;
	WHFRAMEWORK_API bool Encode(const FVoxelGeometryWireHeader& InHeader, TArray<uint8>& OutBytes, FString& OutError);
	WHFRAMEWORK_API bool Decode(TConstArrayView<uint8> InBytes, FVoxelGeometryWireHeader& OutHeader, FString& OutError);
	WHFRAMEWORK_API bool ValidateExpected(const FVoxelGeometryWireHeader& InReply,
		const FVoxelGeometryWireHeader& InRequest, FString& OutError);
}
