#pragma once

#include "CoreMinimal.h"
#include "Main/Base/ModuleBase.h"
#include "Voxel/Interaction/VoxelRaycast.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/VoxelWorldReadiness.h"
#include "VoxelModule.generated.h"

class APlayerController;
class FSaveGameStorage;
class FVoxelCollisionPresenter;
class FVoxelDetailView;
class FVoxelEmergeManager;
class FVoxelGenerationPipeline;
class FVoxelGenerationPlanCache;
class FVoxelInterestManager;
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
	TMap<FString, TArray<uint8>> SceneFiles;
};

UCLASS()
class WHFRAMEWORK_API UVoxelModule : public UModuleBase
{
	GENERATED_BODY()
	GENERATED_MODULE(UVoxelModule)

public:
	UVoxelModule();
	virtual ~UVoxelModule() override;
	static UVoxelModule* Find(UWorld* InWorld);

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
	float GetWarmupProgress() const;
	const FVoxelWorldReadinessSnapshot& GetReadiness() const;
	double BlockSize() const;
	const FVoxelWorldManifest& GetManifest() const;
	FGuid GetSessionId() const;
	uint64 GetWorldEpoch() const;
	FVoxelWorldRuntime* GetRuntime();
	const FVoxelWorldRuntime* GetRuntime() const;
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> GetRegistry() const;
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> GetShapes() const;
	UVoxelMaterialSet* GetMaterialSet() const;
	UVoxelWorldGenerationProfile* GetWorldGenerationProfile() const;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> GetGenerator() const;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> GetGenerationConfig() const;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> GetGenerationCache() const;
	const FVoxelRegionStore& GetRegionStore() const;
	const FVoxelInterestSet& GetCurrentInterest() const;

	FGuid RegisterSource(UObject* InOwner, const FVoxelStreamingSource& InSource);
	bool UpdateSource(const FGuid& InId, const FVoxelStreamingSource& InSource);
	void UnregisterSource(const FGuid& InId);
	void ForceVoxelStreamingRefresh();
	void SetRemoteChangeState(const FIntVector& InSection, EVoxelSectionChangeState InState);
	bool ApplyRemoteRepresentation(const FVoxelRepresentationReply& InReply, FString& OutError);
	void InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate);
	bool RequestRemoteRepresentation(
		EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		uint64 InKnownRevision);
	bool EnqueueNetworkRepresentationTask(FVoxelTaskRequest&& InRequest);
	bool BeginProjectMutation();
	void EndProjectMutation();
	void PublishProjectEdit(const FVoxelEditBatch& InBatch);
	bool IsProjectSceneSimulationEnabled() const;
	AActor* FindSceneActor(const FGuid& InId) const;
	UVoxelSceneRegion* GetSceneRegion(const FIntVector& InSection, bool bCreate);

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
	const FVoxelWorldBasicSaveData& GetWorldBasicData() const;
	EVoxelWorldState GetWorldState() const;
	EVoxelWorldMode GetWorldMode() const;
	void SetWorldMode(EVoxelWorldMode InWorldMode);
	virtual void OnBeforeSaveData() override;
	virtual void OnAfterSaveData(bool bInSuccess) override;
	void SetActiveSaveSource(const FGuid& InSaveId, int32 InGeneration, FSaveGameStorage* InStorage);
	bool CopySaveCapture(FVoxelModuleSaveCapture& OutCapture, FString& OutError) const;
	void SetPendingCommitDirectory(const FString& InDirectory);
	static bool WriteSaveCapture(
		const FVoxelModuleSaveCapture& InCapture,
		const FString& InTemporaryGeneration,
		FString& OutError);

	FVoxelBlocksCommitted OnBlocksCommitted;

	UPROPERTY(BlueprintAssignable)
	FVoxelWorldInitialized OnWorldInitialized;

protected:
	virtual void LoadData(const FParameter& InData, EPhase InPhase) override;
	virtual FParameter GetData() override;
	virtual FParameter ToData() override;
	virtual void UnloadData(EPhase InPhase) override;

	UPROPERTY(EditAnywhere)
	FVoxelWorldBasicSaveData WorldBasicData;

	UPROPERTY(EditAnywhere)
	bool bAutoGenerate = false;

	UPROPERTY(EditAnywhere)
	EVoxelWorldMode WorldMode = EVoxelWorldMode::Default;

	UPROPERTY(Transient)
	EVoxelWorldState WorldState = EVoxelWorldState::None;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UVoxelMaterialSet> MaterialSetAsset;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelMaterialSet> MaterialSet;

	UPROPERTY(EditAnywhere, Category = "Voxel|Generation")
	TSoftObjectPtr<UVoxelWorldGenerationProfile> WorldGenerationProfileAsset;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelWorldGenerationProfile> WorldGenerationProfile;

	UPROPERTY(EditAnywhere, Category = "Voxel|Project")
	bool bEnableProjectSceneSimulation = false;

	UPROPERTY(EditAnywhere, Category = "Voxel|Project")
	TSubclassOf<UVoxelSceneRegion> SceneRegionClass;

	UPROPERTY(Transient)
	TMap<FIntVector, TObjectPtr<UVoxelSceneRegion>> SceneRegions;

	TUniquePtr<FVoxelWorldSaveData> WorldData;

private:
	struct FSource
	{
		TWeakObjectPtr<UObject> Owner;
		FVoxelStreamingSource Value;
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
	void UpdateReadiness();
	TArray<FVector> CollectLocalViewObservers() const;
	TArray<FVector> CollectDetailObservers() const;
	bool IsActorRayClear(const FVector& InStart, const FVector& InEnd, AActor* InIgnore) const;
	bool PlacementOverlapsActors(const FVoxelInteractionPlan& InPlan) const;
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
	TUniquePtr<FVoxelInterestManager> InterestManager;
	TUniquePtr<FVoxelEmergeManager> EmergeManager;
	TUniquePtr<FVoxelResidencyManager> ResidencyManager;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	TUniquePtr<FVoxelViewManager> ViewManager;
	TUniquePtr<FVoxelCollisionPresenter> CollisionPresenter;
	TUniquePtr<FVoxelDetailView> DetailView;
	FVoxelRegionStore RegionStore;
	FVoxelWorldSaveAdapter SaveAdapter;
	FVoxelWorldManifest Manifest;
	FVoxelInterestSet CurrentInterest;
	FVoxelViewSettings ViewSettings;
	FVoxelWorldReadinessSnapshot ReadinessSnapshot;
	EVoxelWorldReadyStage ReadyStage = EVoxelWorldReadyStage::None;
	FGuid SessionId;
	uint64 Epoch = 0;
	TMap<FGuid, FSource> Sources;
	TMap<TWeakObjectPtr<AActor>, FBreak> Breaking;
	TMap<FString, TArray<uint8>> UnloadedSceneFiles;
	TMap<FString, TArray<uint8>> CapturedSceneFiles;
	FString SceneSourceError;
	FString LastSaveError;
	FString PendingCommitDirectory;
	double LastInterestRefresh = -1.0;
	bool bMutating = false;
	bool bWorldLoadRejected = false;
};
