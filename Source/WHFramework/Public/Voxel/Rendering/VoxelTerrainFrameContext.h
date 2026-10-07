#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"

// Module 每帧唯一创建；各阶段只消费同一期限，不重新开始预算。
class WHFRAMEWORK_API FVoxelTerrainFrameContext
{
public:
	bool Configure(const FVoxelTerrainFrameBudgetSettings& InSettings, FString& OutError);
	bool Begin(uint64 InWorldEpoch, uint64 InFrameCounter, double InNowSeconds,
		const FVoxelTerrainFrameObservation& InObservation);
	double Deadline(double InMaximumMilliseconds = MAX_dbl) const;
	double RemainingMilliseconds() const;
	void End(double InNowSeconds);
	void Reset();
	FVoxelPublishBudget& GetPublishBudget();
	const FVoxelTerrainFrameAllocation& GetAllocation() const;
	double GetLastTerrainMilliseconds() const;
	uint64 GetWorldEpoch() const;
	uint64 GetFrameCounter() const;
	bool IsActive() const;
	bool IsOverdueOpportunityAvailable() const;
	void ConsumeOverdueOpportunity() const;

private:
	FVoxelTerrainFrameBudget Controller;
	FVoxelPublishBudget SharedBudget;
	FVoxelTerrainFrameAllocation Allocation;
	uint64 WorldEpoch = 0;
	uint64 FrameCounter = 0;
	double StartedSeconds = 0.0;
	double DeadlineSeconds = 0.0;
	double LastTerrainMilliseconds = 0.0;
	bool bActive = false;
	mutable bool bOverdueOpportunityUsed = false;
};
