#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class UVoxelModule;
class UVoxelCollisionComponent;
struct FVoxelSectionCollisionResult;
struct FVoxelCollisionEvidenceNode;
struct FVoxelSectionSnapshot;

struct WHFRAMEWORK_API FVoxelPhysicsReceipt
{
	uint64 WorldEpoch = 0;
	FGuid SceneEpoch;
	int32 PhysicsStep = INDEX_NONE;
	FIntVector Section = FIntVector::ZeroValue;
	uint64 SectionToken = 0;
	uint64 DataRevision = 0;
	uint64 ContentRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	uint64 BodyGeneration = 0;
	uint64 SectionIncarnation = 0;
	FGuid PresenterOwnerId;

	bool MatchesInput(const FVoxelTaskStamp& InInput, const FGuid& InSceneEpoch) const;
};

// 物理线程执行同步标记后形成的不可变证据根，供安全范围读者保持。
struct WHFRAMEWORK_API FVoxelCollisionCoverageSnapshot
{
	TSharedPtr<const FVoxelCollisionEvidenceNode, ESPMode::ThreadSafe> Root;
	uint64 WorldEpoch = 0;
	uint64 Serial = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	const FVoxelPhysicsReceipt* Find(const FIntVector& InSection) const;
};

class WHFRAMEWORK_API FVoxelCollisionPresenter
{
public:
	FVoxelCollisionPresenter(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch);
	void Tick(const TMap<FIntVector, FVoxelExactDemand>& InDemand, uint64 InInterestRevision);
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InSection);
	bool EnsureNavigationDemand(
		const FIntVector& InSection);
	bool IsReady(const FIntVector& InSection) const;
	bool CaptureCurrentReceipt(const FIntVector& InSection, FVoxelPhysicsReceipt& OutReceipt) const;
	TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> CaptureSnapshot() const;
	void Reset();

private:
	struct FPhysicsSync
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TAtomic<bool> bComplete{false};
		TAtomic<bool> bCommitted{false};
		TAtomic<int32> Step{INDEX_NONE};
	};
	struct FPhysicsSyncCommand;
	struct FControlState;
	struct FBodyRecord
	{
		TStrongObjectPtr<UVoxelCollisionComponent> Component;
		TStrongObjectPtr<UVoxelCollisionComponent> PreparingComponent;
		TStrongObjectPtr<UVoxelCollisionComponent> RetiringComponent;
		TSharedPtr<FVoxelSectionCollisionResult, ESPMode::ThreadSafe> Prepared;
		TSharedPtr<FVoxelSectionCollisionResult, ESPMode::ThreadSafe> SyncPrepared;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InstalledLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparingLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiringLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EvidenceLease;
		TSharedPtr<FPhysicsSync, ESPMode::ThreadSafe> Sync;
		FVoxelTaskStamp PreparedStamp;
		FVoxelTaskStamp InstalledStamp;
		FVoxelTaskStamp RequestedStamp;
		FVoxelTaskStamp FailedStamp;
		FVoxelPhysicsReceipt Receipt;
		FIntVector Section = FIntVector::ZeroValue;
		FBodyRecord* Previous = nullptr;
		FBodyRecord* Next = nullptr;
		uint64 QueueSerial = 0;
		int32 BoxCursor = 0;
		double QueuedAt = 0.0;
		bool bInstalled = false;
		bool bRetiring = false;
		bool bHasFailure = false;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AdmissionInputLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AdmissionOutputLease;
		TSharedPtr<FVoxelSectionSnapshot, ESPMode::ThreadSafe> AdmissionSnapshot;
		TOptional<FVoxelTaskRequest> AdmissionRequest;
		FVoxelGenerationConsumerKey AdmissionConsumer;
		FVoxelGenerationConsumerKey CapacityConsumer;
		uint64 CapacityVersion = 0;
		bool bCapacityBlocked = false;
	};
	FBodyRecord* FindBody(const FIntVector& InSection) const;
	FBodyRecord* FindOrAddBody(const FVoxelGenerationConsumerKey& InConsumer);
	void RemoveBody(FBodyRecord& InBody);
	void RequestCollision(const FIntVector& InSection, uint64 InRevision, const FVoxelExactDemand& InDemand);
	FVoxelTaskStamp CaptureInputStamp(const FIntVector& InSection) const;
	TSharedPtr<FPhysicsSync, ESPMode::ThreadSafe> PreparePhysicsSync(const FIntVector& InSection);
	void AdvanceBody(const FIntVector& InSection, FBodyRecord& InBody);
	void InstallEvidence(const FIntVector& InSection, FBodyRecord& InBody, bool bInRetired);
	bool IsDemanded(const FIntVector& InSection) const;
	void BeginRetirement(FBodyRecord& InBody);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	FGuid OwnerId = FGuid::NewGuid();
	FGuid SceneEpoch = FGuid::NewGuid();
	uint64 NextBodyGeneration = 0;
	uint64 NextBodyQueueSerial = 0;
	uint64 NextLeaseSerial = 0;
	uint64 CoverageSerial = 0;
	int32 DemandCursor = 0;
	TSharedPtr<FControlState, ESPMode::ThreadSafe> Control;
	TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> Coverage;
	FVoxelTerrainWorkSelection Selection;
	double StepCostMicroseconds = 20.0;
};
