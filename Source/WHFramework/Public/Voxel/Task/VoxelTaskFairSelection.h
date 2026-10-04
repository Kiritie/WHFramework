#pragma once

#include "CoreMinimal.h"

// 此类只选择现有 Scheduler 的任务，不创建线程池、不持有另一份任务内容。
enum class EVoxelTaskServiceLane : uint8
{
	None = 0,
	Critical,
	Exact,
	Fine,
	ProxyData,
	SurfaceData,
	MacroData,
	Coverage,
	Transition,
	ProxyMesh,
	SurfaceMesh,
	MacroMesh,
	Other,
	Count
};

enum class EVoxelTaskBlockReason : uint16
{
	None = 0,
	Dependency = 1 << 0,
	Obsolete = 1 << 1,
	SourceRemoved = 1 << 2,
	GlobalSlots = 1 << 3,
	LaneSlots = 1 << 4,
	Memory = 1 << 5,
	InputMemory = 1 << 6,
	RetryDelay = 1 << 7,
	TerminalFailure = 1 << 8,
	PendingCapacity = 1 << 9,
	Oversized = 1 << 10
};

struct WHFRAMEWORK_API FVoxelTaskServiceCandidate
{
	int32 PendingIndex = INDEX_NONE;
	EVoxelTaskServiceLane Lane = EVoxelTaskServiceLane::None;
	EVoxelTaskBlockReason Blocked = EVoxelTaskBlockReason::None;
	int32 SourcePriority = 0;
	double Distance = 0.0;
	double EnqueuedAt = 0.0;
	uint64 Sequence = 0;
	bool bPublicationContinuation = false;
};

class WHFRAMEWORK_API FVoxelTaskFairSelection
{
public:
	int32 Select(TConstArrayView<FVoxelTaskServiceCandidate> InCandidates, double InNow);
	void Reset();
	void SetFairness(int32 InCriticalBurst, double InMaximumBypassSeconds);

private:
	static bool Better(const FVoxelTaskServiceCandidate& InA,
		const FVoxelTaskServiceCandidate& InB, double InNow, double InAgeLimit);
	uint64 LastServed[static_cast<uint8>(EVoxelTaskServiceLane::Count)] = {};
	uint64 DispatchSerial = 0;
	int32 CriticalBurst = 0;
	int32 CriticalDependencyBurst = 0;
	int32 MaximumCriticalBurst = 4;
	double MaximumBypassSeconds = 0.5;
};
