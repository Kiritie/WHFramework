#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

struct FVoxelFineShardGeometry;

struct WHFRAMEWORK_API FVoxelFineMeshInput
{
	uint64 WorldEpoch = 0;
	uint64 Incarnation = 0;
	uint64 GeneratorIdentity = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	uint64 DataRevision = 0;
	uint64 OverlayRevision = 0;
	uint64 MeshRevision = 0;
	FVoxelFineShardKey Shard;

	bool operator==(const FVoxelFineMeshInput& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelFineBuildAttempt
{
	uint64 Serial = 0;
	FVoxelFineMeshInput Input;

	bool operator==(const FVoxelFineBuildAttempt& InOther) const = default;
};

enum class EVoxelFineAttemptDisposition : uint8
{
	None = 0,
	Accepted,
	Stale,
	Canceled,
	Failed
};

// 仅管理 Desired/Prepared 构建；实际 Presented 由唯一逐 Atom Coverage 状态持有。
class WHFRAMEWORK_API FVoxelFineBuildState
{
public:
	bool Initialize(const FVoxelFineMeshInput& InInput, FString& OutError);
	FVoxelFineBuildAttempt BeginAttempt();
	EVoxelFineAttemptDisposition Complete(const FVoxelFineBuildAttempt& InAttempt, bool bInCanceled,
		TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> InGeometry, FString InError);
	bool NotifyContentChanged(uint64 InGeneratorIdentity, uint64 InRegistryRevision, uint64 InShapeRevision,
		uint64 InDataRevision, uint64 InOverlayRevision);
	void NotifyDataChanged(uint64 InDataRevision, uint64 InOverlayRevision);
	void NotifyMeshPolicyChanged(uint64 InRegistryRevision, uint64 InShapeRevision);
	void NotifyTopologyChanged();
	bool IsCurrent(const FVoxelFineBuildAttempt& InAttempt) const;
	bool IsMeshReady() const;
	bool IsKnownEmpty() const;
	const FVoxelFineMeshInput& GetInput() const;
	const TOptional<FVoxelFineBuildAttempt>& GetPendingAttempt() const;
	const TOptional<FVoxelFineBuildAttempt>& GetPreparedAttempt() const;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> GetPreparedGeometry() const;
	const FString& GetFailure() const;
	uint64 GetTopologyRevision() const;

private:
	void InvalidatePrepared();
	FVoxelFineMeshInput Input;
	TOptional<FVoxelFineBuildAttempt> Pending;
	TOptional<FVoxelFineBuildAttempt> Prepared;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> PreparedGeometry;
	uint64 NextAttempt = 0;
	uint64 TopologyRevision = 0;
	bool bPreparedEmpty = false;
	FString Failure;
};
