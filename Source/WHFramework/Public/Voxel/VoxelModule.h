#pragma once
#include "Voxel/Navigation/VoxelNavigationPresenter.h"

#include "CoreMinimal.h"
#include "Main/Base/ModuleBase.h"
#include "Voxel/Interaction/VoxelRaycast.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Save/VoxelSceneSaveCapture.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Streaming/VoxelSimulationResidency.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/VoxelWorldReadiness.h"
#include "Voxel/VoxelWorldReadinessRuntime.h"
#include "VoxelModule.generated.h"

class APlayerController;
struct FAbilityItem;
class FSaveGameStorage;
struct FVoxelFineOverlayCapture;
struct FVoxelFineOverlayReadResult;
struct FVoxelFineOverlayVersionProof;
struct FVoxelOverlayCommitNotification;
class FVoxelCollisionPresenter;
class FVoxelPhysicsRetirement;
class FVoxelNavigationPresenter;
class FVoxelDetailView;
class FVoxelEmergeManager;
class FVoxelGenerationPipeline;
class FVoxelGenerationPlanCache;
class IVoxelGenerationOverlay;
class FVoxelInterestRuntime;
class FVoxelMapTileCache;
class FVoxelOverlayReplica;
class UVoxelModuleNetworkComponent;
class FVoxelResidencyManager;
class FVoxelShapeRegistry;
class FVoxelTaskScheduler;
struct FVoxelRepresentationReply;
struct FVoxelRepresentationInvalidate;
struct FVoxelRepresentationWireKey;
enum class EVoxelRepresentationWireType : uint8;
class FVoxelViewManager;
class FVoxelWorldRuntime;
class UAbilityInventoryBase;
class UVoxelMaterialSet;
class UVoxelSceneRegion;
class UVoxelWorldGenerationProfile;
struct FVoxelGenerationRuntimeConfig;
struct FVoxelPrefabSaveData;
struct FVoxelTaskResult;
enum class EVoxelSectionChangeState : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelBlocksCommitted, const FVoxelEditBatch&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVoxelWorldInitialized);

struct WHFRAMEWORK_API FVoxelModuleSaveCapture
{
	FVoxelWorldSaveCapture Voxels;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> Scenes;
};

UCLASS()
class WHFRAMEWORK_API UVoxelModule : public UModuleBase
{
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelNavigationResidencyTest;
	friend class FVoxelNavigationNativeTileTest;
	friend class FVoxelFineShardAttemptIsolationTest;
	friend class FVoxelSourceAcceptanceBarrierTest;
	friend class FVoxelCriticalDependencyAdmissionTest;
	friend class FVoxelPublishEstimateRecoveryTest;
	friend class FVoxelInterestInputBudgetTest;
	friend class FVoxelNetworkWorldControlTest;
#endif
	friend class UVoxelSceneRegion;
	GENERATED_BODY()
	GENERATED_MODULE(UVoxelModule)

public:
	UVoxelModule();
	virtual ~UVoxelModule() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& InCollector);

#if WITH_EDITOR
	virtual void OnDestroy() override;
#endif

	virtual UWorld* GetWorld() const override;
	virtual void OnInitialize() override;
	virtual void OnPreparatory(EPhase InPhase) override;
	virtual void OnRefresh(float InDeltaSeconds, bool bInEditor) override;
	virtual void OnTermination(EPhase InPhase) override;
	virtual bool IsSaveEnabled() const override;

	bool CreateWorld(const FVoxelGenerationSettings& InSettings, int32 InBlockSizeCentimeters, FString& OutError);
	bool CreateWorldFromProfile(int32 InSeed, FString& OutError);
	bool StartWorld(const FVoxelWorldManifest& InManifest, bool bInRemote, FString& OutError);
	bool StopWorld(bool bDiscardDirty, FString& OutError);
	bool ValidateWorldData(const FParameter& InData, FString& OutError) const;

	bool IsAuthority() const;
	bool IsReady() const;
	bool IsPlayable() const;
	bool IsCollisionReady(const FIntVector& InSection) const;
	bool CaptureCollisionReceipt(const FIntVector& InSection, FVoxelPhysicsReceipt& OutReceipt) const;
	void NotifyCollisionChanged(const FIntVector& InSection);
	bool IsNavigationReady(const FIntVector& InSection) const;
	bool IsNavigationInputCurrent(const FVoxelNavigationInputVersion& InVersion) const;
	bool IsNavigationResident(
		const FIntVector& InSection) const;
	bool EnsureNavigationCollisionSection(
		const FIntVector& InSection);
	bool AcquireNavigationDataSection(
		const FIntVector& InSection,
		FVoxelRuntimeSectionPin& OutPin);
	void ReleaseNavigationDataSection(
		const FIntVector& InSection,
		const FVoxelRuntimeSectionPin& InPin);

	UPROPERTY(EditAnywhere, Category = "Voxel|Navigation", meta = (ClampMin = "1.0"))
	double NavigationClearanceCentimeters = 180.0;
	bool IsSimulationResident(const FIntVector& InSection) const;
	TSet<FIntVector> GetSimulationSections() const;
	bool RetrySimulationSection(const FIntVector& InSection);
	uint64 GetSimulationResidencyRevision() const;
	float GetWarmupProgress() const;
	const FVoxelWorldReadinessSnapshot& GetReadiness() const;
	FVoxelRepresentationReadiness GetRepresentationReadiness() const;
	FVoxelTerrainReadinessSnapshot CaptureTerrainReadiness() const;
	double BlockSize() const;
	const FVoxelWorldManifest& GetManifest() const;
	FGuid GetSessionId() const;
	uint64 GetWorldEpoch() const;
	uint64 GetInterestRevision() const;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> GetCurrentInterestSnapshot() const;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> GetDesiredInterestSnapshot() const;
	bool CanCommitTerrainInterest(const FVoxelInterestSet& InSnapshot) const;
	void CommitTerrainInterest(TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InSnapshot);
	void AbandonTerrainInterest(const FVoxelInterestSet& InSnapshot);
	void NotifyTerrainSectionReplacement(const FIntVector& InSection, uint64 InWorldEpoch);
	bool IsTerrainSafeAt(const FGuid& InSource, const FBox& InWorldBounds) const;
	FVoxelWorldRuntime* GetRuntime();
	const FVoxelWorldRuntime* GetRuntime() const;
	bool CaptureFineOverlays(const FVoxelGenerationBounds& InBounds, FVoxelFineOverlayCapture& OutCapture, FString& OutError,
		const FVoxelGenerationConsumerKey* InConsumer = nullptr) const;
	bool IsOverlayReadCurrent(const FVoxelFineOverlayReadResult& InResult) const;
	bool IsOverlayVersionCurrent(const TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe>& InProof) const;
	FVoxelOverlayReplica* GetOverlayReplica() const;
	void RegisterTerrainNetwork(UVoxelModuleNetworkComponent* InComponent);
	void UnregisterTerrainNetwork(UVoxelModuleNetworkComponent* InComponent);
	void QueueTerrainWorldControl(UVoxelModuleNetworkComponent* InComponent);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GetTerrainFactResources() const;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GetTerrainResources() const;
	FVoxelGenerationConsumerKey GetTerrainTaskControlConsumer() const;
	TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> GetPhysicsRetirement();
	FVoxelGenerationDependencyStatus EnsureGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGenerationBounds& InBounds, const FVoxelTerrainServicePriority& InPriority);
	void ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer);
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> GetRegistry() const;
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> GetShapes() const;
	UVoxelMaterialSet* GetMaterialSet() const;
	UVoxelWorldGenerationProfile* GetWorldGenerationProfile() const;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> GetGenerator() const;
	FVoxelGenerationDependencyStatus QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample);
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	bool ResolveMapSurface(
		const FVector2D& InMapPosition,
		FVector& OutLocation,
		FString& OutError) const;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> GetGenerationConfig() const;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> GetGenerationCache() const;
	virtual TMap<FName, FVoxelPlanDiagnostics> GetPlanDiagnostics() const;
	FVoxelMapTileCache* GetMapTileCache() const;
	const FVoxelRegionStore& GetRegionStore() const;
	const FVoxelInterestSet& GetCurrentInterest() const;
	FVoxelTaskDiagnostics GetTaskDiagnostics() const;
	TSharedPtr<const FVoxelTerrainStatistics, ESPMode::ThreadSafe> GetTerrainStatistics() const;
	FVoxelPrimaryFineReadiness QueryFineReadiness(TConstArrayView<FIntVector> InKeys) const;
	const FVoxelViewSettings& GetViewSettings() const { return ViewSettings; }
	bool EnqueueProjectBackgroundTask(FVoxelTaskRequest&& InRequest);
	void CancelProjectTasksByOwner(const FGuid& InOwnerId, uint64 InOwnerIncarnation);
	void DonateProjectTaskPriority(const FVoxelTaskStamp& InStamp, EVoxelWorkClass InClass,
		int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	void SetPersistenceEnabled(bool bInEnabled);
	bool IsPersistenceEnabled() const;
	TSoftObjectPtr<UVoxelWorldGenerationProfile> GetWorldGenerationProfileAsset() const;
	int32 GetDefaultWorldSeed() const;

	FGuid RegisterSource(UObject* InOwner, const FVoxelStreamingSource& InSource);
	bool UpdateSource(const FGuid& InId, const FVoxelStreamingSource& InSource);
	void UnregisterSource(const FGuid& InId);
	bool PreparePlayerClaim(const FGuid& InSource, UObject* InPawn, const FGuid& InPawnIncarnation,
		const FIntVector& InCandidateCell, FVoxelPlayerClaimToken& OutToken);
	bool CommitPlayerClaim(const FVoxelPlayerClaimToken& InToken, UObject* InPawn);
	void AbortPlayerClaim(const FVoxelPlayerClaimToken& InToken);
	bool IsPlayerClaimCommitted(const FVoxelPlayerClaimToken& InToken, const UObject* InPawn) const;
	bool IsSourceOwnedBy(const FGuid& InSource, const UObject* InOwner) const;
	bool IsSourceAdmitted(const FGuid& InId) const;
	bool IsSourceAdmittedAt(const FGuid& InId, const FIntVector& InCell, uint64 InEpoch) const;
	void CancelProjectTask(const FVoxelTaskStamp& InStamp);
	void CollectStreamingSourcesForOwner(const AActor* InOwner, TArray<FVoxelStreamingSource>& OutSources) const;
	FVoxelStreamingReadiness QueryStreamingReadiness(const FGuid& InId) const;
	FVoxelFrameTimings GetFrameTimings() const;
	FVoxelTerrainFrameContext& GetTerrainFrameContext();
	FVoxelStreamingReadiness QueryBoundsReadiness(const FVoxelGenerationBounds& InBounds, bool bInRequireCollision) const;
	void ForceVoxelStreamingRefresh();
	void SetRemoteChangeState(const FIntVector& InSection, EVoxelSectionChangeState InState);
	bool ApplyRemoteRepresentation(const FVoxelRepresentationReply& InReply, FString& OutError);
	void InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate);
	bool RequestRemoteRepresentation(
		EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		uint64 InKnownRevision);
	bool EnqueueNetworkRepresentationTask(FVoxelTaskRequest&& InRequest);
	void CancelTerrainNetworkTask(const FVoxelTaskStamp& InStamp);
	bool BeginProjectMutation();
	void EndProjectMutation();
	void PublishProjectEdit(const FVoxelEditBatch& InBatch);
	bool IsProjectSceneSimulationEnabled() const;
	AActor* FindSceneActor(const FGuid& InId) const;
	UVoxelSceneRegion* GetSceneRegion(const FIntVector& InSection, bool bCreate);
	// Only for transferring an already live actor. Never requests data or simulation.
	UVoxelSceneRegion* GetSceneRegionForActor(AActor* InActor, const FIntVector& InSection);

	FVoxelTraceResult Trace(const FVector& InStart, const FVector& InDirection, double InDistance = 600.0) const;
	bool VerifyView(
		APlayerController* InController,
		AActor* InAuthorizedObserver,
		const FVoxelEditIntent& InIntent,
		FVoxelTraceResult& OutTrace,
		FString& OutError) const;
	FVoxelEditReply ExecuteIntent(
		APlayerController* InController,
		AActor* InSource,
		const FVoxelEditIntent& InIntent,
		bool bTrustedStandaloneCreative = false);
	bool ApplyPrefab(const FVoxelPrefabSaveData& InPrefab, const FIntVector& InOrigin, FString& OutError);
	bool ExportPrefab(
		const FIntVector& InMin,
		const FIntVector& InMax,
		FVoxelPrefabSaveData& OutPrefab,
		FString& OutError) const;

	virtual TUniquePtr<FVoxelWorldSaveData> NewWorldData(const FParameter& InBasic = FParameter()) const;
	const FVoxelWorldSaveData& GetWorldData() const;
	EVoxelWorldState GetWorldState() const;
	virtual void OnBeforeSaveData() override;
	virtual void OnAfterSaveData(bool bInSuccess) override;
	EVoxelSceneCaptureStep GetSaveCaptureState(FString& OutError) const;
	void AdvanceSaveCapture();
	double GetMaximumSceneCaptureStepMilliseconds() const;
	bool DidApplySaveCommit() const;
	void SetActiveSaveSource(const FGuid& InSaveId, int32 InGeneration, FSaveGameStorage* InStorage);
	bool CopySaveCapture(FVoxelModuleSaveCapture& OutCapture, FString& OutError) const;
	bool BindSaveCommitOwner(const FGuid& InSaveId, const FString& InStorageRoot, FString& OutError);
	bool BindSaveCommitTarget(const FGuid& InSaveId, int32 InGeneration, const FString& InDirectory, FString& OutError);
	static bool WriteSaveCapture(
		const FVoxelModuleSaveCapture& InCapture,
		const FString& InTemporaryGeneration,
		FString& OutError);

	FVoxelBlocksCommitted OnBlocksCommitted;

	UPROPERTY(BlueprintAssignable)
	FVoxelWorldInitialized OnWorldInitialized;

protected:
	virtual uint64 GetSceneProjectCaptureRevision() const;
	virtual void OnWorldStopping();
	virtual void OnSimulationResidencyChanged(const FVoxelSimulationResidencyDelta& InDelta);
	virtual bool BuildInteractionPlan(
		const FVoxelTraceResult& InHit,
		EVoxelEditAction InAction,
		uint16 InPlaceType,
		const FVector& InViewDirection,
		AActor* InSource,
		FVoxelInteractionPlan& OutPlan,
		FString& OutError);
	virtual void OnInteractionCommitted(
		const FVoxelInteractionPlan& InPlan,
		AActor* InSource);

	virtual bool ValidateInteractionPlan(
		FVoxelInteractionPlan& InOutPlan,
		FString& OutError);

	virtual void OnContainerTransferCommitted(
		APlayerController* InController,
		AActor* InSource,
		const FIntVector& InAnchor,
		const FAbilityItem& InTransferredItem,
		bool bInTaken);

	/** Project-specific authored content that changes the generated base world. */
	virtual uint64 GetGenerationIdentitySalt() const;

	virtual bool CreateGenerationOverlay(
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InNaturalGenerator,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe>& OutOverlay,
		FString& OutError) const;

	virtual void LoadData(const FParameter& InData, EPhase InPhase) override;
	virtual FParameter GetData() override;
	virtual FParameter ToData() override;
	virtual void UnloadData(EPhase InPhase) override;

	UPROPERTY(EditAnywhere)
	bool bAutoGenerate = false;

	UPROPERTY(Transient)
	EVoxelWorldState WorldState = EVoxelWorldState::None;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UVoxelMaterialSet> MaterialSetAsset;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelMaterialSet> MaterialSet;

	UPROPERTY(EditAnywhere, Category = "Voxel|Generation")
	TSoftObjectPtr<UVoxelWorldGenerationProfile> WorldGenerationProfileAsset;

	UPROPERTY(EditAnywhere, Category = "Voxel|Generation")
	int32 DefaultWorldSeed = 1;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelWorldGenerationProfile> WorldGenerationProfile;

	UPROPERTY(EditAnywhere, Category = "Voxel|Project")
	bool bEnableProjectSceneSimulation = false;

	UPROPERTY(EditAnywhere, Category = "Voxel|Project")
	TSubclassOf<UVoxelSceneRegion> SceneRegionClass;

	UPROPERTY(EditAnywhere, Category = "Voxel|Persistence", meta = (ClampMin = "0.01"))
	double MaximumSceneCaptureStepMilliseconds = 1.0;

	UPROPERTY(Transient)
	TMap<FIntVector, TObjectPtr<UVoxelSceneRegion>> SceneRegions;

	TUniquePtr<FVoxelWorldSaveData> WorldData;

private:
	struct FSource
	{
		TWeakObjectPtr<UObject> Owner;
		FVoxelStreamingSource Value;
		FVoxelStreamingSource PlannedValue;
		FIntVector LastAcceptedPosition = FIntVector::ZeroValue;
		uint64 CandidateRevision = 1;
		uint64 ControlRevision = 0;
		uint64 ContractRevision = 1;
		FVoxelPlayerClaimState PlayerClaim;
		FVoxelGameThreadContinuation ClaimVerification;
		uint64 ClaimContractRevision = 0;
		uint64 ClaimContentRevision = 0;
		uint64 ClaimRenderSerial = 0;
		uint64 ClaimPhysicsSerial = 0;
		uint64 ClaimSimulationRevision = 0;
	};

	struct FBreak
	{
		FIntVector Target = FIntVector::ZeroValue;
		FVoxelBlockState Expected;
		double Began = 0.0;
		double LastPulse = 0.0;
	};

	void ApplyTask(FVoxelTaskResult&& InResult);
	void RefreshInterest(double InNow);
	void QueueSimulationDemandEvents(TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> InEvents);
	void ReconcileSimulationDemand();
	void WakeSimulationSection(const FIntVector& InSection);
	void OnSimulationSectionEvent(const FIntVector& InSection, const FVoxelSectionStamp& InStamp, EVoxelRuntimeSectionEvent InEvent);
	void QueueSimulationSection(FVoxelSection& InSection);
	void UnqueueSimulationSection(FVoxelSection& InSection);
	void HoldSimulationSectionForRegion(FVoxelSection& InSection, UVoxelSceneRegion& InRegion);
	void ReleaseSimulationSectionFromRegion(FVoxelSection& InSection);
	void HoldSimulationSectionForSave(FVoxelSection& InSection, UVoxelSceneRegion& InRegion);
	void ReleaseSimulationSectionFromSave(FVoxelSection& InSection);
	bool ReviveSimulationSection(FVoxelSection& InSection, UVoxelSceneRegion& InRegion);
	void FinishSimulationSectionRetirement(FVoxelSection& InSection);
	void ApplySceneSaveReceipt(UVoxelSceneRegion& InRegion, double InDeadline);
	void RequestSceneDurableSave();
	bool HasUnpersistedSceneState() const;
	bool ReserveSimulationStorage(int32 InSectionCount, int32 InRegionCount);
	void RegisterSceneRegion(UVoxelSceneRegion& InRegion);
	void UnregisterSceneRegion(UVoxelSceneRegion& InRegion);
	void AdvanceSceneRegions();
	void NotifySceneCaptureChanged();
	EVoxelSceneCaptureStep AdvanceSaveCaptureStep();
	void OnSectionEvicting(const FIntVector& InSection);
	bool ActivateSimulationSection(const FIntVector& InSection);
	void RemoveSimulationSection(const FIntVector& InSection);
	mutable double NextTerrainGeometryLogSeconds = 0.0;
	void LogTerrainGeometryHistogram() const;
	void UpdateReadiness();
	void AdvancePlayerClaims();
	FVoxelGameThreadContinuation VerifyPlayerClaimSteps(FVoxelPlayerClaimToken InToken,
		TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InInterest);
	TArray<FVector> CollectLocalViewObservers() const;
	TArray<FVector> CollectDetailObservers() const;
	bool IsActorRayClear(const FVector& InStart, const FVector& InEnd, AActor* InIgnore) const;
	bool PlacementOverlapsActors(const FVoxelInteractionPlan& InPlan) const;
	bool ApplyGenerationProfileFromSave(
		const FVoxelWorldSaveData& InData,
		FString& OutError);
	UAbilityInventoryBase* ResolveInventory(APlayerController* InController, AActor* InSource) const;
	FVoxelEditReply TransferContainer(
		APlayerController* InController,
		AActor* InSource,
		const FVoxelEditIntent& InIntent,
		const FVoxelTraceResult& InHit);

private:
	FVoxelRegistry Registry;
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> GenerationConfig;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> GenerationCache;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TUniquePtr<FVoxelWorldRuntime> Runtime;
	TSharedPtr<const FVoxelInterestRuntime, ESPMode::ThreadSafe> InterestRuntime;
	TSharedPtr<const FVoxelInterestRuntime, ESPMode::ThreadSafe> DesiredInterestRuntime;
	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> DesiredInterestTaskPayload;
	TUniquePtr<FVoxelEmergeManager> EmergeManager;
	TUniquePtr<FVoxelResidencyManager> ResidencyManager;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	TUniquePtr<FVoxelOverlayReplica> OverlayReplica;
	TArray<TWeakObjectPtr<UVoxelModuleNetworkComponent>> TerrainNetworkComponents;
	TWeakObjectPtr<UVoxelModuleNetworkComponent> PendingTerrainWorldControl;
	int32 TerrainNetworkCursor = 0;
	TQueue<TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe>> OverlayChangeQueue;
	TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> ApplyingOverlayChange;
	int32 OverlayChangeCursor = 0;
	TUniquePtr<FVoxelMapTileCache> MapTileCache;
	TUniquePtr<FVoxelViewManager> ViewManager;
	TUniquePtr<FVoxelCollisionPresenter> CollisionPresenter;
	TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> PhysicsRetirement;
	TUniquePtr<FVoxelNavigationPresenter> NavigationPresenter;
	TUniquePtr<FVoxelDetailView> DetailView;
	FVoxelRegionStore RegionStore;
	FVoxelWorldSaveAdapter SaveAdapter;
	FVoxelWorldManifest Manifest;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> CurrentInterest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> DesiredInterest = CurrentInterest;
	FVoxelViewSettings ViewSettings;
	FVoxelTerrainFrameContext TerrainFrameContext;
	FVoxelWorldReadinessSnapshot ReadinessSnapshot;
	FVoxelWorldReadinessRuntime ReadinessRuntime;
	EVoxelWorldReadyStage ReadyStage = EVoxelWorldReadyStage::None;
	FGuid SessionId;
	uint64 Epoch = 0;
	mutable uint64 NextOverlayCaptureSerial = 0;
	TMap<FGuid, FSource> Sources;
	TMap<TWeakObjectPtr<AActor>, FBreak> Breaking;
	TMap<FString, TArray<uint8>> UnloadedSceneFiles;
	TOptional<FVoxelSceneSaveCaptureBuilder> SceneSaveBuilder;
	uint64 SceneCaptureRevision = 1;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> CapturedSceneSave;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> LatestSceneSaveAttempt;
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> DurableSceneSaveReceipt;
	FString LastSaveError;
	FString PendingCommitDirectory;
	uint64 InterestRevision = 0;
	uint64 SourceRevision = 0;
	uint64 InterestAttemptSerial = 0;
	uint64 SafetyCaptureSerial = 0;
	FGuid TerrainPlanningOwnerId = FGuid::NewGuid();
	uint64 LastPlannedRenderCoverageSerial = 0;
	uint64 LastPlannedPhysicsCoverageSerial = 0;
	TSet<FIntVector> SimulationSections;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SimulationStorageLease;
	TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> SimulationDemandEvents;
	const FVoxelResidencyDemandEvents* SimulationDemandEventsTail = nullptr;
	FVoxelSection* SimulationReadyHead = nullptr;
	FVoxelSection* SimulationReadyTail = nullptr;
	UVoxelSceneRegion* SceneRegionHead = nullptr;
	UVoxelSceneRegion* SceneRegionTail = nullptr;
	UVoxelSceneRegion* SceneRegionCursor = nullptr;
	int32 SimulationSectionCapacity = 0;
	int32 SceneRegionCapacity = 0;
	int32 SceneLoadingCount = 0;
	uint64 SceneSourceIncarnation = 1;
	int32 SimulationDemandCursor = 0;
	bool bReadingRemovedSimulation = false;
	bool bSceneTransitionActive = false;
	bool bSceneSaveRequested = false;
	bool bLatestSceneSaveSucceeded = false;
	int32 SimulationSaveWaitCount = 0;
	double SceneSaveCaptureCostMilliseconds = 0.1;
	double SimulationEventCostMilliseconds = 0.005;
	double SimulationStepCostMilliseconds = 0.03;
	double SceneRegionCostMilliseconds = 0.03;
	uint64 SimulationResidencyRevision = 0;
	double LastInterestRefresh = -1.0;
	double LastDiagnosticsLog = -1.0;
	TArray<double> DiagnosticFrameTimes;
	TArray<double> DiagnosticModuleTimes;
	double LastEmergeAdmissionMilliseconds = 0.0;
	uint64 NextSourceRegistrationOrder = 0;
	bool bInterestDirty = true;
	bool bInterestBuildPending = false;
	bool bMutating = false;
	bool bWorldLoadRejected = false;
	bool bPersistenceEnabledForCurrentWorld = false;
};
