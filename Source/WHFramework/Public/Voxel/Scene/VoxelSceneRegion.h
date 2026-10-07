#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "VoxelSceneRegion.generated.h"

class UVoxelModule;
struct FVoxelSavedSceneActor;
struct FVoxelSection;
struct FVoxelSceneColumnCapture;
struct FVoxelSceneColumnCaptureCursor;
struct FVoxelSceneColumnLoadAttempt;
class FVoxelTaskScheduler;
class FSaveGameGenerationReadLease;
class FVoxelTerrainResourceLease;
enum class EVoxelSceneCaptureStep : uint8;

UCLASS(Abstract)
class WHFRAMEWORK_API UVoxelSceneRegion : public UWHObject, public ISceneContainerInterface
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	virtual void Initialize(UVoxelModule* InModule, const FIntVector& InRegionKey);
	virtual void Shutdown();
	virtual void OnSectionActivated(const FIntVector& InSection);
	virtual void OnSectionDeactivated(const FIntVector& InSection);
	bool TryDeactivateSection(const FIntVector& InSection, TArray<uint8>& OutRegionBytes, FString& OutError,
		bool* OutWaitingForScene = nullptr);
	virtual void TickSceneActors(float InDeltaSeconds);
	virtual bool CaptureProjectData(FParameter& OutData) const;
	virtual bool CaptureProjectDataForActors(TConstArrayView<FVoxelSavedSceneActor> InActors, FParameter& OutData) const;
	virtual EVoxelSceneCaptureStep AdvanceProjectSaveCapture(FVoxelSceneColumnCaptureCursor& InCursor, FString& OutError) const;
	virtual bool RestoreProjectData(const FParameter& InData, FString& OutError);
	virtual bool ShouldPersistActor(AActor* InActor) const;

	bool CaptureActors(TArray<uint8>& OutBytes, FString& OutError) const;
	EVoxelSceneCaptureStep AdvanceSavePayloads(FVoxelSceneColumnCaptureCursor& InCursor, FString& OutError) const;
	bool RestoreActors(TConstArrayView<uint8> InBytes, FString& OutError);

	virtual bool HasSceneActor(const FString& InId, bool bInEnsured = true) const override;
	virtual AActor* GetSceneActor(
		const FString& InId,
		TSubclassOf<AActor> InClass = nullptr,
		bool bInEnsured = true) const override;
	virtual bool AddSceneActor(AActor* InActor) override;
	virtual bool RemoveSceneActor(AActor* InActor) override;
	virtual void DestroySceneActors() override;

	const FIntVector& GetRegionKey() const;
	const TMap<FGuid, TObjectPtr<AActor>>& GetSceneActors() const;
	bool HasActiveSections() const;
	bool HasStoredActor(const FGuid& InId) const;
	const TMap<FGuid, FParameter>& GetPendingActors() const;

	bool bSceneReady = false;
	bool bSceneLoading = false;
	bool bSceneFailed = false;

protected:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PendingActorStorageLease;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelModule> Module = nullptr;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<AActor>> SceneActorMap;

	UPROPERTY(Transient)
	TMap<FGuid, FParameter> PendingActors;

	FIntVector RegionKey = FIntVector::ZeroValue;
	TSet<FIntVector> ActiveSections;

private:
	friend class UVoxelModule;
	bool BeginSceneLoad(FVoxelTaskScheduler& InScheduler,
		TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> InSource, uint32 InExpectedBytes);
	void AdvanceSceneLoad(FVoxelTaskScheduler& InScheduler);
	void DetachSceneLoad();
	void RetrySceneLoad();
	TSharedPtr<FVoxelSceneColumnLoadAttempt, ESPMode::ThreadSafe> SceneLoadAttempt;
	uint64 SceneLoadSerial = 0;
	UVoxelSceneRegion* PreviousRegion = nullptr;
	UVoxelSceneRegion* NextRegion = nullptr;
	FVoxelSection* SimulationFailureHead = nullptr;
	FVoxelSection* SimulationFailureTail = nullptr;
	FVoxelSection* SimulationFailureCursor = nullptr;
	FVoxelSection* SimulationSaveHead = nullptr;
	FVoxelSection* SimulationSaveTail = nullptr;
	FGuid SceneInstanceId;
	uint64 SceneRevision = 0;
	uint64 DurableSceneRevision = 0;
	bool bIdleSaveWaiting = false;
	bool bSceneSaveFailed = false;
	FGuid IgnoredSceneSaveFailureId;
	bool bRegisteredRegion = false;
	double LastSceneTickWorldSeconds = 0.0;
	bool CaptureActorStates(TArray<FVoxelSavedSceneActor>& OutActors, FParameter& OutProjectData, FString& OutError) const;
	bool CaptureInactiveActors(FString& OutError, const FIntVector* InRetiringSection = nullptr,
		TConstArrayView<FVoxelSavedSceneActor> InFrozenActors = {},
		const TFunctionRef<bool(TConstArrayView<FGuid>)>* InBeforeCommit = nullptr,
		bool* OutWaitingForScene = nullptr);
	bool RestorePendingActor(const FGuid& InId, const FParameter& InData);
	void MarkSceneChanged();
	float ResidencyRefreshSeconds = 0.0f;
	int32 PendingActorCursor = 0;
};
