#pragma once

#include "CoreMinimal.h"

enum class EVoxelPublishPhase : uint8
{
	None = 0,
	Plan,
	Priority,
	Admission,
	Prepare,
	Commit,
	Retire,
	Count
};

struct WHFRAMEWORK_API FVoxelPublishPhaseTiming
{
	double Milliseconds = 0.0;
	double MaximumOperationMilliseconds = 0.0;
	int32 Operations = 0;
	int32 Deferrals = 0;
};

// 一帧只有一个截止时间；不可中断的单次调用超时必须单独计量。
class WHFRAMEWORK_API FVoxelPublishBudget
{
public:
	void Begin(double InNowSeconds, double InTotalMilliseconds, int32 InMaximumOperations);
	bool TryStart(EVoxelPublishPhase InPhase, double InNowSeconds, double InEstimatedMilliseconds);
	void Finish(EVoxelPublishPhase InPhase, double InStartedSeconds, double InFinishedSeconds);
	double RemainingMilliseconds(double InNowSeconds) const;
	double TotalMilliseconds(double InNowSeconds) const;
	bool HasOverrun(double InNowSeconds) const;
	const FVoxelPublishPhaseTiming& GetTiming(EVoxelPublishPhase InPhase) const;

private:
	double Started = 0.0;
	double Deadline = 0.0;
	int32 RemainingOperations = 0;
	FVoxelPublishPhaseTiming Timings[static_cast<uint8>(EVoxelPublishPhase::Count)];
};
