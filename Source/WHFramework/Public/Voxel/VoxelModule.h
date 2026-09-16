#pragma once
#include "CoreMinimal.h"
#include "Main/Base/ModuleBase.h"
#include "Voxel/Interaction/VoxelRaycast.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Runtime/VoxelStreaming.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Tasks/VoxelTaskScheduler.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelModule.generated.h"

class UVoxelModuleNetworkComponent;
class UVoxelChunk;
class UVoxelMaterialSet;
class APlayerController;
class UAbilityInventoryBase;
class UVoxelAgentComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelBlocksCommitted, const FVoxelEditBatch&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVoxelRemoteBatchCompleted, const FVoxelSnapshotBatch&, bool);
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

	virtual ~UVoxelModule();

	//////////////////////////////////////////////////////////////////////////
public:
#if WITH_EDITOR
	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase Phase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnTermination(EPhase Phase) override;

	//////////////////////////////////////////////////////////////////////////
public:
	virtual UWorld* GetWorld() const override;

	virtual bool IsSaveEnabled() const override;

	virtual void OnBeforeSaveData() override;

	virtual void OnAfterSaveData(bool bSuccess) override;

	//////////////////////////////////////////////////////////////////////////
public:
	bool BeginProjectMutation()
	{
		if (bMutating || !IsAuthority() || !IsReady())
			return false;
		bMutating = true;
		return true;
	}
	void EndProjectMutation()
	{
		bMutating = false;
	}
	void PublishProjectEdit(const FVoxelEditBatch& Batch)
	{
		OnBlocksCommitted.Broadcast(Batch);
	}

	bool CreateWorld(const FVoxelGenerationSettings& Settings, int32 BlockSizeCentimeters, FString& Error);

	bool StartWorld(const FVoxelWorldManifest& Manifest, bool bFromServer, FString& Error);

	bool StopWorld(bool bDiscardDirty, FString& Error);

	bool ValidateWorldData(const FParameter& Data, FString& Error) const;

	bool IsAuthority() const;

	bool IsReady() const
	{
		return bool(Runtime) && WorldState == EVoxelWorldState::Running;
	}

	float GetWarmupProgress() const;

	double BlockSize() const
	{
		return Manifest.BlockSizeCentimeters;
	}
	const FVoxelWorldManifest& GetManifest() const
	{
		return Manifest;
	}
	FGuid GetSessionId() const
	{
		return SessionId;
	}
	FVoxelWorldRuntime* GetRuntime()
	{
		return Runtime.Get();
	}
	const FVoxelWorldRuntime* GetRuntime() const
	{
		return Runtime.Get();
	}
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> GetRegistry() const
	{
		return Registry.GetSnapshot();
	}
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> GetShapes() const
	{
		return Shapes;
	}
	UVoxelMaterialSet* GetMaterialSet() const
	{
		return MaterialSet;
	}
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> GetGenerator() const
	{
		return Generator;
	}

	UVoxelChunk* GetColumn(FIntPoint Key, bool bCreate = false);

	AActor* FindSceneActor(const FGuid& ID) const;
	FGuid RegisterSource(UObject* Owner, const FVoxelStreamingSource& Source);
	bool UpdateSource(const FGuid& ID, const FVoxelStreamingSource& Source);
	void UnregisterSource(const FGuid& ID);
	bool QueueRemoteEncoded(const TArray<uint8>& Payload, FString& Error);
	bool QueueNetworkEncode(const FVoxelSnapshotBatch& Batch, UVoxelModuleNetworkComponent* Recipient);
	bool QueueRemoteBatch(const FVoxelSnapshotBatch& Batch, FString& Error);
	bool CopyOverlay(const FVoxelSectionKey& Key, FVoxelSectionOverlay& Out) const;
	FVoxelTraceResult Trace(const FVector& Start, const FVector& Direction, double Distance = 600) const;
	bool VerifyView(APlayerController* PC, AActor* AuthorizedObserver, const FVoxelEditIntent& Intent, FVoxelTraceResult& Out, FString& Error) const;
	FVoxelEditReply ExecuteIntent(APlayerController* PC, AActor* Source, const FVoxelEditIntent& Intent, bool bTrustedStandaloneCreative = false);
	bool ApplyPrefab(const FVoxelPrefabSaveData& Prefab, const FIntVector& Origin, FString& Error);
	bool ExportPrefab(const FIntVector& Min, const FIntVector& Max, FVoxelPrefabSaveData& Out, FString& Error) const;
	virtual TUniquePtr<FVoxelWorldSaveData> NewWorldData(const FParameter& Basic = FParameter()) const;
	const FVoxelWorldSaveData& GetWorldData() const
	{
		return *WorldData;
	}
	const FVoxelWorldBasicSaveData& GetWorldBasicData() const
	{
		return WorldBasicData;
	}
	EVoxelWorldState GetWorldState() const
	{
		return WorldState;
	}
	EVoxelWorldMode GetWorldMode() const
	{
		return WorldMode;
	}

	void SetWorldMode(EVoxelWorldMode InWorldMode);

	void SetActiveSaveSource(const FGuid& SaveId, int32 Generation, FSaveGameStorage* Storage);

	bool CopySaveCapture(FVoxelModuleSaveCapture& Out, FString& Error) const;
	void SetPendingCommitDirectory(const FString& Directory)
	{
		PendingCommitDirectory = Directory;
	}
	static bool WriteSaveCapture(const FVoxelModuleSaveCapture& Capture, const FString& TempGeneration, FString& Error);

	//////////////////////////////////////////////////////////////////////////
public:
	FVoxelBlocksCommitted OnBlocksCommitted;

	FVoxelRemoteBatchCompleted OnRemoteBatchCompleted;

	UPROPERTY(BlueprintAssignable)
	FVoxelWorldInitialized OnWorldInitialized;

	//////////////////////////////////////////////////////////////////////////
protected:
	virtual void LoadData(const FParameter& Data, EPhase Phase) override;

	virtual FParameter GetData() override;

	virtual FParameter ToData() override;

	virtual void UnloadData(EPhase Phase) override;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere)
	FVoxelWorldBasicSaveData WorldBasicData;

	UPROPERTY(EditAnywhere)
	bool bAutoGenerate = false;

	UPROPERTY(EditAnywhere)
	EVoxelWorldMode WorldMode = EVoxelWorldMode::Default;
	UPROPERTY(Transient)
	EVoxelWorldState WorldState = EVoxelWorldState::None;
	UPROPERTY(EditAnywhere)
	TSubclassOf<UVoxelChunk> ChunkSpawnClass;
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UVoxelMaterialSet> MaterialSetAsset;
	UPROPERTY(Transient)
	TObjectPtr<UVoxelMaterialSet> MaterialSet;
	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<UVoxelChunk>> Columns;
	TUniquePtr<FVoxelWorldSaveData> WorldData;

	//////////////////////////////////////////////////////////////////////////
private:
	struct FSource
	{
		TWeakObjectPtr<UObject> Owner;
		FVoxelStreamingSource Value;
	};
	struct FBreak
	{
		FIntVector Target;
		FVoxelBlockState Expected;
		double Began = 0;
		double LastPulse = 0;
	};

	//////////////////////////////////////////////////////////////////////////
private:
	void RefreshStreaming(double Now);

	void QueueSection(const FVoxelSectionKey& Key, const FVoxelSectionDemand& Demand);
	void ApplyTask(FVoxelTaskResult&& Result);
	void PumpRemote();
	bool CaptureColumnForUnload(UVoxelChunk& Column, FString& Error);
	void QueueSceneLoad(UVoxelChunk& Column, const FVoxelSection& Section);
	bool IsActorRayClear(const FVector& Start, const FVector& End, AActor* Ignore) const;
	bool PlacementOverlapsActors(const FVoxelInteractionPlan& Plan) const;
	UAbilityInventoryBase* ResolveInventory(APlayerController* PC, AActor* Source) const;
	FVoxelEditReply TransferContainer(APlayerController* PC, AActor* Source, const FVoxelEditIntent& Intent, const FVoxelTraceResult& Hit);
	FVoxelRegistry Registry;
	TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TUniquePtr<FVoxelWorldRuntime> Runtime;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	FVoxelRegionStore RegionStore;
	FVoxelWorldSaveAdapter SaveAdapter;

	FVoxelWorldManifest Manifest;
	FGuid SessionId;
	uint64 Epoch = 0;
	TMap<FGuid, FSource> Sources;
	TMap<FVoxelSectionKey, FVoxelSectionDemand> Desired;
	TArray<FVoxelSectionKey> OrderedDesired;
	int32 DispatchCursor = 0;
	TOptional<FVoxelSnapshotBatch> RemoteInFlight;
	TMap<FVoxelSectionKey, double> RetryAfter;
	TMap<FVoxelSectionKey, int32> RetryCount;
	TArray<FVoxelSnapshotBatch> RemotePending;
	TArray<TArray<uint8>> EncodedRemotePending;
	bool bDecodeRunning = false;
	uint64 NextNetworkJob = 1;
	int32 SceneTickCursor = 0;
	TArray<FIntPoint> SceneColumnOrder;
	TMap<uint64, TWeakObjectPtr<UVoxelModuleNetworkComponent>> NetworkRecipients;
	TMap<uint64, FGuid> NetworkBatchIds;

	void PumpDecode();

	TMap<FString, TArray<uint8>> UnloadedSceneFiles;

	TMap<FString, TArray<uint8>> CapturedSceneFiles;
	TMap<TWeakObjectPtr<AActor>, FBreak> Breaking;
	FString LastSaveError;
	FString PendingCommitDirectory;
	double LastStreaming = -1;
	bool bMutating = false;
	bool bRemoteRunning = false;
};
