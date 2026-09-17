#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
enum class EVoxelTaskKind : uint8
{
    Generate, LoadDelta, RebuildOverlay, Mesh, Collision, SaveEncode,
    LoadScene, EncodeNetwork, DecodeNetwork,
    BuildProxy, BuildViewMesh, BuildViewPage, BuildViewPlan, BuildDetails
};
inline bool VoxelIsDerivedTask(EVoxelTaskKind K)
{
    return K == EVoxelTaskKind::BuildProxy || K == EVoxelTaskKind::BuildViewMesh ||
        K == EVoxelTaskKind::BuildViewPage || K == EVoxelTaskKind::BuildViewPlan ||
        K == EVoxelTaskKind::BuildDetails;
}
struct WHFRAMEWORK_API FVoxelTaskStamp
{
    uint64 WorldEpoch = 0, GenerationToken = 0, Revision = 0, GeometryVersion = 0;
    FVoxelSectionKey Key;
    bool operator==(const FVoxelTaskStamp& B) const
    {
        return WorldEpoch == B.WorldEpoch && GenerationToken == B.GenerationToken &&
            Revision == B.Revision && GeometryVersion == B.GeometryVersion && Key == B.Key;
    }
};
