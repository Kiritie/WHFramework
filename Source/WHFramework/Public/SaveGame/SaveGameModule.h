#pragma once

#include "SaveGame/SaveOperationTypes.h"
#include "SaveGame/SaveSlotTypes.h"
#include "SaveGame/SaveManifestTypes.h"
#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameLoadSnapshot.h"
#include "SaveGame/SaveGameStorage.h"
#include "SaveGame/SaveGameAsyncExecutor.h"

#include "SaveGameModule.generated.h"

class FSaveGameLocalContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWorldSaveFinished, FGuid, SaveId, const FSaveOperationResult&, Result);

using FSaveWorldLoadGuard = TFunction<bool(const FGuid&, const TMap<FName, FParameter>&, EPhase, FString&)>;

UCLASS()
class WHFRAMEWORK_API USaveGameModule : public UModuleBase
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelSaveCoordinatorTest;
	friend class FSaveGameTransientSessionTest;
#endif

	GENERATED_MODULE(USaveGameModule)

public:
	USaveGameModule();

	// 全部模块文件验证后、任何模块应用前执行准备；应用后执行一致性验证。
	bool RegisterWorldLoadGuard(FName InName, FSaveWorldLoadGuard InBefore, FSaveWorldLoadGuard InAfter);
	void UnregisterWorldLoadGuard(FName InName);
	bool RegisterWorldStoreParticipant(FName InName, FSaveWorldStoreParticipant InParticipant);
	void UnregisterWorldStoreParticipant(FName InName);
	FString GetStorageRoot() const;
	FString GetUnboundInventoryRoot() const;

protected:
	void RequireWorldStoreParticipant(FName InName);
	void RequireWorldLoadGuard(FName InName);

public:

	UFUNCTION(BlueprintPure, Category = "SaveGame|Session")
	bool IsTransientSession() const
	{
		return (bSessionPolicyLocked ? RuntimeSessionPolicy : SessionPolicy) == ESaveSessionPolicy::Transient;
	}

	static bool IsStepTimeLimitEnforced();
	~USaveGameModule();

	//////////////////////////////////////////////////////////////////////////
public:
#if WITH_EDITOR
	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;
	virtual void OnPreparatory(EPhase InPhase) override;
	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;
	virtual void OnControlRefresh(float RealDeltaSeconds) override;
	virtual void OnTermination(EPhase InPhase) override;
	virtual void PrepareForHostTermination() override;

protected:
	virtual void OnSaveTerminationComplete() { }

	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual FParameter ToData() override;

public:
	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveActiveSlot();

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveSlotAsync(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveActiveSlotAsync();

	FSaveOperationResult PollPendingSave();

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	FSaveOperationResult GetWorldSaveResult(const FGuid& InOperationId) const;
	void AdvanceWorldSaveCapture(UVoxelModule& InVoxel);
	bool HasWorldSaveWork(const UVoxelModule& InVoxel) const;
	bool RequestUserQuit(APlayerController* InPlayer, uint8 InQuitPreference, bool bInIgnorePlatformRestrictions);

	UPROPERTY(BlueprintAssignable)
	FWorldSaveFinished OnWorldSaveFinished;

	FGuid BeginPendingSaveSlot(const FCreateSaveSlotParams& Params);
	bool CancelPendingSaveSlot(const FGuid& InToken);
	bool IsPendingSaveSlot(const FGuid& InToken) const;
	FSaveOperationResult SavePendingSlot(const FGuid& InToken);
	FSaveOperationResult GetPendingSlotResult(const FGuid& InToken) const;
	bool ConsumePendingSlotResult(const FGuid& InToken, FSaveOperationResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	bool HasPendingSaveSlot() const
	{
		return !IsTransientSession() && bHasPendingSaveSlot;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveCurrentSlot();

	/** Detach the live preview/runtime from a slot without forgetting which committed slot was last used. */
	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	void DetachActiveSave();

	/** Forget the active slot both in memory and across the next process start. */
	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	void ClearActiveSave();

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult LoadSlot(FGuid SaveId, EPhase InPhase = EPhase::All);

	FSaveOperationResult RequestLoadSlot(FGuid SaveId, EPhase InPhase, const FGuid& InOperationId);
	FSaveOperationResult GetLoadOperationResult(const FGuid& InOperationId) const;
	const FSaveOperationResult& GetLastLoadOperationResult() const { return LoadOperationResult; }
	void CancelLoadOperation(const FGuid& InOperationId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult DeleteSaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult RenameSaveSlot(FGuid SaveId, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	bool HasActiveSave() const
	{
		return !IsTransientSession() && ActiveSaveId.IsValid();
	}

	bool IsSaveOperationRunning() const
	{
		return bSaveOperationRunning || bApplyingSaveCallbacks || !AsyncCreatedSlotCleanup.IsCompleted();
	}

	// Native cleanup of an already cancelled temporary slot does not retain World suspension.
	bool IsWorldSaveTransactionRunning() const
	{
		return (bSaveOperationRunning && !bApplyingWorldLoad) || bSaveTerminationRunning;
	}

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	FGuid GetActiveSaveId() const
	{
		return IsTransientSession() ? FGuid() : ActiveSaveId;
	}

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	FSaveSlotSummary GetSaveSlotSummary(FGuid SaveId) const;

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	TArray<FSaveSlotSummary> GetSaveSlotSummaries() const;

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Profile")
	FSaveOperationResult SaveProfile(FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Profile")
	FSaveOperationResult LoadProfile(FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintPure)
	int32 GetUserIndex() const
	{
		return UserIndex;
	}

	UFUNCTION(BlueprintCallable)
	void SetUserIndex(int32 InUserIndex);

private:
	FSaveOperationResult PollNativeSave(FVoxelTerrainFrameContext* InFrame);
	void AdvanceAsyncSaveCapture(FVoxelTerrainFrameContext* InFrame = nullptr);
	bool AdvanceModuleFileCapture(FString& OutError, bool& bOutCapacityBlocked);
	void ResetModuleFileCapture();
	bool CancelPendingSaveSlotForCleanup(const FGuid& InToken);
	void CompleteAsyncSave(const FSaveOperationResult& Result);
	void AdvanceAsyncSaveCompletion(FVoxelTerrainFrameContext* InFrame = nullptr, bool bForce = false);
	void FinalizeAsyncSave();
	void CompleteUserQuit(const FSaveOperationResult& InResult);
	void RestoreLastActiveSave();
	void LockSessionPolicy();
	void AdoptPendingSaveLoad(FSaveGameLoadSnapshot InSnapshot);
	FSaveOperationResult LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation, EPhase InPhase);
	FSaveOperationResult RestoreLoadSnapshot(EPhase InPhase);
	TArray<UModuleBase*> GetSaveModules(
		ESaveScope Scope,
		bool bRequireSaveEnabled = true) const;
	void OnGameExited(UObject* InSender, const struct FEventGameExited& InEvent);

private:
	/** 临时会话不读写世界存档和设置 Profile；运行中更改此配置仅对下次创建的模块实例生效。 */
	UPROPERTY(EditAnywhere, Category = "SaveGame|Session")
	ESaveSessionPolicy SessionPolicy = ESaveSessionPolicy::Persistent;

	// 非反射的运行态快照只属于该模块实例；配置修改不会改变在途存档的所有权。
	ESaveSessionPolicy RuntimeSessionPolicy = ESaveSessionPolicy::Persistent;
	bool bSessionPolicyLocked = false;

	UPROPERTY(EditAnywhere, Category = "UserData")
	int32 UserIndex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame", meta = (AllowPrivateAccess = "true"))
	FGuid ActiveSaveId;

	UPROPERTY(Transient)
	bool bSaveOperationRunning;

	UPROPERTY(Transient)
	bool bHasPendingSaveSlot;

	UPROPERTY(Transient)
	FCreateSaveSlotParams PendingSaveSlotParams;

	FGuid PendingSaveSlotToken;
	FGuid AsyncPendingSaveSlotToken;
	TMap<FGuid, FSaveOperationResult> PendingSlotResults;
	bool bPendingSlotAdoptionInProgress = false;

	FSaveGameLoadSnapshot LoadSnapshot;

	FSaveOperationResult LoadOperationResult;

	TUniquePtr<FSaveGameStorage> Storage;
	TSharedPtr<const FSaveGameLocalContext, ESPMode::ThreadSafe> LocalSaveContext;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AsyncExecutorLease;
	TUniquePtr<FSaveGameAsyncExecutor> AsyncSave;
	UE::Tasks::FTask AsyncCreatedSlotCleanup;
	TSharedPtr<FSaveGenerationPlan, ESPMode::ThreadSafe> AsyncPlan;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AsyncCaptureMetadataLease;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UModuleBase>> AsyncCaptured;

	UPROPERTY(Transient)
	TObjectPtr<UWorld> AsyncWorld;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelModule> AsyncVoxelOwner;

	UPROPERTY(Transient)
	FParameter AsyncModuleData;

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AsyncModulePayloadLease;
	TArray<uint8> AsyncModulePayload;
	TOptional<FSaveModuleBytes> AsyncModuleFile;
	uint8 AsyncModuleCapturePhase = 0;
	int32 AsyncModulePayloadCursor = 0;
	uint32 AsyncModulePayloadCrc = 0;

	int32 AsyncBuildCursor = 0;
	int32 AsyncPrepareCount = 0;
	TOptional<FSaveOperationResult> AsyncCompletionResult;
	int32 AsyncCompletionCursor = -1;
	bool bAsyncCompletionFilesCommitted = false;
	bool bApplyingSaveCallbacks = false;
	bool bApplyingWorldLoad = false;
	bool bSaveTerminationRunning = false;
	bool bHostTerminationRequested = false;
	double AsyncCaptureCostMilliseconds = 0.1;
	double AsyncCompletionCostMilliseconds = 0.1;
	FGuid AsyncOperationId;
	FSaveOperationResult LastWorldSaveResult;
	FGuid PendingCreatedSaveId;
	TOptional<FSaveManifest> PendingCreatedManifest;
	FGuid PendingCreationPreviousSaveId;
	bool bAsyncCompletesPendingSlot = false;
	bool bAsyncWorkerStarted = false;
	bool bAsyncWorkerCapacityBlocked = false;
	uint64 AsyncWorkerBlockedReleaseVersion = 0;
	FGuid PendingSlotSaveId;
	bool bPendingUserQuit = false;
	bool bQuitNeedsFreshCapture = false;
	bool bQuitSaveCommitted = false;
	bool bQuitWorldWasPaused = false;
	bool bQuitSaveModuleWasPaused = false;
	bool bQuitVoxelModuleWasPaused = false;
	uint8 PendingQuitPreference = 0;
	bool bPendingQuitIgnorePlatformRestrictions = false;
	TWeakObjectPtr<APlayerController> QuitController;
	TWeakObjectPtr<UVoxelModule> QuitVoxel;
	FGuid AsyncSaveId;
private:
    struct FWorldLoadGuardEntry
    {
        FSaveWorldLoadGuard Before;
        FSaveWorldLoadGuard After;
    };
    TMap<FName, FWorldLoadGuardEntry> WorldLoadGuards;
    TSet<FName> RequiredWorldLoadGuards;
    FName RequiredWorldStoreParticipant;
    bool CheckRequiredWorldParticipants(FString& OutError) const;
    bool RunWorldLoadGuards(const FGuid& InSaveId, const TMap<FName, FParameter>& InModules, EPhase InPhase, bool bInAfter, FString& OutError);
    FName WorldStoreParticipantName;
    FSaveWorldStoreParticipant WorldStoreParticipant;
    FSaveWorldStoreParticipant AsyncStoreParticipant;
    TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe> AsyncExternalOperation;
    bool BeginExternalStore(FSaveWorldCaptureContext InContext, const TMap<FName, FParameter>& InSourceModules,
        FSaveWorldStoreParticipant& OutParticipant, TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe>& OutOperation, FString& OutError);
    bool FinishExternalStore(const FSaveWorldStoreParticipant& InParticipant,
        const TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe>& InOperation, bool bInFilesCommitted, FString& OutError);
};

