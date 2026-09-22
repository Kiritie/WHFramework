#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

class AActor;
class UVoxelModule;
class UVoxelMeshComponent;

struct WHFRAMEWORK_API FVoxelViewCoverageResult final : FVoxelTaskCustomPayload
{
	TArray<TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>> Meshes;
	uint64 OwnedMeshBytes = 0;
	virtual uint64 GetAllocatedBytes() const override;
};

class WHFRAMEWORK_API FVoxelViewPublisher
{
public:
	FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch);
	~FVoxelViewPublisher();
	bool Stage(AActor*& InOutActor, const FVector& InLocation, double InScale, FVoxelSectionMeshResult&& InMesh);
	void BeginBatch();
	void SetCoverage(AActor* InActor, TArray<FBox> InWorldCellBoxes);
	void SetHidden(AActor* InActor, bool bInHidden);
	bool EndBatch(TFunction<void()> InOnCommitted = {});
	void Tick();
	void Forget(AActor* InActor);
	void Reset();
	bool IsBusy() const;
	bool NeedsUpdate() const;
	bool IsPresented(AActor* InActor) const;
	bool IsCommitted(AActor* InActor) const;
	bool HasPresentation(AActor* InActor) const;

private:
	using FMeshPtr = TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>;

	struct FEntry
	{
		FMeshPtr Source;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		FBox Bounds = FBox(ForceInit);
		TArray<FBox> Exclusions;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
		bool bDirty = true;
		bool bPresented = false;
	};

	struct FUpdate
	{
		TWeakObjectPtr<AActor> Actor;
		FMeshPtr Source;
		FVector Location = FVector::ZeroVector;
		double Scale = 1.0;
		TArray<FBox> Exclusions;
		TArray<TObjectPtr<UVoxelMeshComponent>> Components;
	};

	void AdmitBuilds();
	bool PrepareUpdate(int32 InIndex);
	void CommitBatch();
	void DiscardBatch();
	void CommitVisibility();

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 BatchSerial = 0;
	TMap<TWeakObjectPtr<AActor>, FEntry> Entries;
	TArray<FUpdate> Updates;
	TMap<TWeakObjectPtr<AActor>, bool> Visibility;
	TArray<FMeshPtr> PreparedMeshes;
	TFunction<void()> OnCommitted;
	int32 BuildIndex = 0;
	int32 PendingBuilds = 0;
	int32 PrepareIndex = 0;
	bool bBusy = false;
	bool bRetry = false;
};
