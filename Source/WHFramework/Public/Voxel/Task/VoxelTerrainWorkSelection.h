#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Task/VoxelTerrainResources.h"

enum class EVoxelTerrainServiceBand : uint8
{
	None = 0,
	P0,
	P1,
	P2,
	P3,
	P4,
	P5,
	P6,
	Count
};

enum class EVoxelTerrainServicePool : uint8
{
	None = 0,
	Near,
	NonNear,
	All,
	Count
};

struct WHFRAMEWORK_API FVoxelTerrainServicePriority
{
	EVoxelTerrainServiceBand Band = EVoxelTerrainServiceBand::P6;
	double DistanceCells = MAX_dbl;
	double ForwardScore = 0.0;
	int32 SourcePriority = MAX_int32;
	FGuid SourceStableKey;

	bool operator==(const FVoxelTerrainServicePriority& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelTerrainServiceCandidate
{
	int32 Index = INDEX_NONE;
	FVoxelTerrainServicePriority Priority;
	double FirstQueuedSeconds = 0.0;
	uint64 Sequence = 0;
	double EstimatedMicroseconds = 1000.0;
	bool bRunnable = false;
};

struct WHFRAMEWORK_API FVoxelTerrainServiceCapacity
{
	int32 MaximumWorkers = 1;
	int32 RunningWorkers = 0;
	int32 RunningNearWorkers = 0;
	int32 NearReservedWorkers = 1;
	int32 MinimumNonNearWorkers = 1;
	double MaximumWaitSeconds = 0.5;
	bool bAllowOverdueOpportunity = false;
	double MaximumOverdueMicroseconds = 1000.0;
};

struct WHFRAMEWORK_API FVoxelTerrainServiceCharge
{
	uint64 Serial = 0;
	EVoxelTerrainServicePool Pool = EVoxelTerrainServicePool::None;
	EVoxelTerrainServiceBand Band = EVoxelTerrainServiceBand::None;
	double EstimatedMicroseconds = 0.0;
	bool bOverdueOpportunity = false;
};

// 只持有选择账本；由既有Scheduler持有队列、线程与任务生命周期。
class WHFRAMEWORK_API FVoxelTerrainWorkSelection
{
public:
	FVoxelTerrainWorkSelection();
	~FVoxelTerrainWorkSelection();
	FVoxelTerrainWorkSelection(const FVoxelTerrainWorkSelection&) = delete;
	FVoxelTerrainWorkSelection& operator=(const FVoxelTerrainWorkSelection&) = delete;
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool Prepare(FString& OutError);
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return Admission; }
	const FVoxelTerrainResourceLease* GetResourceLease() const { return ResourceLease.Get(); }
	int32 Select(TConstArrayView<FVoxelTerrainServiceCandidate> InCandidates,
		const FVoxelTerrainServiceCapacity& InCapacity, double InNowSeconds,
		FVoxelTerrainServiceCharge& OutCharge);
	bool Settle(uint64 InChargeSerial, double InActualMicroseconds);
	bool RefundBeforeStart(uint64 InChargeSerial);
	bool Reset();
	bool HasAccountingFault() const;
	static bool IsValidCost(double InMicroseconds);
	static bool HigherPriority(const FVoxelTerrainServicePriority& InFirst, const FVoxelTerrainServicePriority& InSecond);
	static FVoxelTerrainServicePriority Promote(const FVoxelTerrainServicePriority& InFirst, const FVoxelTerrainServicePriority& InSecond);
	static bool IsNear(EVoxelTerrainServiceBand InBand);

private:
	static constexpr int32 BandCount = static_cast<int32>(EVoxelTerrainServiceBand::Count);
	static constexpr int32 PoolCount = static_cast<int32>(EVoxelTerrainServicePool::Count);

	struct FPoolState
	{
		double Deficits[BandCount] = {};
		uint64 DispatchCounts[BandCount] = {};
		int32 Cursor = 1;
		bool bVisitOpen = false;
	};

	bool TryChangeCredit(FPoolState& InOutState, int32 InBand, double InDelta);
	bool ReserveCharges();

	static constexpr double MaximumTaskCostMicroseconds = 1000000000.0;
	static constexpr double MaximumAccountingMagnitude = 281474976710656.0;
	static constexpr int32 MaximumOutstandingCharges = 4096;
	FPoolState Pools[PoolCount];
	TMap<uint64, FVoxelTerrainServiceCharge> OutstandingCharges;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	int32 ChargeCapacity = 0;
	uint64 NextChargeSerial = 0;
	bool bAccountingFault = false;
	EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
};
