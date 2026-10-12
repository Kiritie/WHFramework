#pragma once

#include "Step/StepModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "StepModule.generated.h"

class UStepBase;
class UStepAsset;
class UStepRuntime;

UCLASS()
class WHFRAMEWORK_API UStepModule : public UModuleBase
{
	GENERATED_BODY()
	GENERATED_MODULE(UStepModule)

public:
	UStepModule();
	~UStepModule();

#if WITH_EDITOR
	virtual void OnGenerate() override;
	virtual void OnDestroy() override;
#endif
	virtual void OnInitialize() override;
	virtual void OnPreparatory(EPhase InPhase) override;
	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;
	virtual void OnPause() override;
	virtual void OnUnPause() override;
	virtual void OnTermination(EPhase InPhase) override;
	virtual FString GetModuleDebugMessage() override;
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual void UnloadData(EPhase InPhase) override;
	virtual FParameter ToData() override;

	UFUNCTION(BlueprintPure)
	UStepAsset* GetAsset(UStepAsset* InAsset) const;

	UFUNCTION(BlueprintPure)
	UStepRuntime* GetRuntime(UStepAsset* InAsset) const;

	UFUNCTION(BlueprintCallable)
	void AddAsset(UStepAsset* InAsset);

	UFUNCTION(BlueprintCallable)
	void RemoveAsset(UStepAsset* InAsset);

	UFUNCTION(BlueprintCallable)
	void SwitchAsset(UStepAsset* InAsset);

	UFUNCTION(BlueprintCallable)
	void StartStep(int32 InRootStepIndex = -1, bool bSkipSteps = false);

	UFUNCTION(BlueprintCallable)
	void StartStepByGUID(const FString& InRootGUID, bool bSkipSteps = false);

	UFUNCTION(BlueprintCallable)
	void EndStep(bool bRestoreSteps = false);

	UFUNCTION(BlueprintCallable)
	void RestoreStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void EnterStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void RefreshStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void GuideStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void ExecuteStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void LeaveStep(UStepBase* InStep);

	UFUNCTION(BlueprintPure)
	bool IsAllStepCompleted() const;

	UFUNCTION(BlueprintPure)
	EStepModuleState GetStepModuleState() const { return StepModuleState; }

	UFUNCTION(BlueprintPure)
	TArray<UStepAsset*> GetAssets() const;

	UFUNCTION(BlueprintPure)
	UStepAsset* GetDefaultAsset() const { return DefaultAsset; }

	UFUNCTION(BlueprintPure)
	UStepAsset* GetCurrentAsset() const;

	UFUNCTION(BlueprintPure)
	UStepBase* GetFirstStep() const;

	UFUNCTION(BlueprintPure)
	UStepBase* GetCurrentStep() const { return CurrentStep; }

	UFUNCTION(BlueprintPure)
	UStepBase* GetCurrentRootStep() const;

	UFUNCTION(BlueprintPure)
	TArray<UStepBase*> GetRootSteps() const;

	UFUNCTION(BlueprintPure)
	TMap<FString, UStepBase*> GetStepMap() const;

	UFUNCTION(BlueprintPure)
	UStepBase* GetStepByGUID(const FString& InStepGUID) const;

	UFUNCTION(BlueprintPure)
	EStepExecuteType GetGlobalStepExecuteType() const { return GlobalStepExecuteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepExecuteType(EStepExecuteType InType);

	UFUNCTION(BlueprintPure)
	EStepCompleteType GetGlobalStepCompleteType() const { return GlobalStepCompleteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepCompleteType(EStepCompleteType InType);

	UFUNCTION(BlueprintPure)
	EStepLeaveType GetGlobalStepLeaveType() const { return GlobalStepLeaveType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepLeaveType(EStepLeaveType InType);

	bool Owns(const UStepBase* InStep) const;
	bool IsShuttingDown() const { return bTerminating || bPendingClear || bClearingRuntimes; }

protected:
	virtual void BroadcastStepStarted(int32 RootIndex);
	virtual void BroadcastStepEnded();

	UPROPERTY(EditAnywhere, Category = "StepModule")
	UStepAsset* DefaultAsset = nullptr;

	UPROPERTY(Transient)
	TArray<UStepRuntime*> Runtimes;

	UPROPERTY(Transient)
	UStepRuntime* CurrentRuntime = nullptr;

	UPROPERTY(Transient)
	UStepBase* CurrentStep = nullptr;

	UPROPERTY(Transient)
	EStepModuleState StepModuleState = EStepModuleState::None;

	UPROPERTY(Transient)
	int32 CurrentRootStepIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepExecuteType GlobalStepExecuteType = EStepExecuteType::None;

	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepCompleteType GlobalStepCompleteType = EStepCompleteType::None;

	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepLeaveType GlobalStepLeaveType = EStepLeaveType::None;

private:
	friend struct FStepCallbackScope;
	friend class UStepBase;
	void SkipUnmatchedStep(UStepBase* InStep);
	UPROPERTY(Transient)
	TArray<FStepDeferredCommand> DeferredCommands;
	int32 CallbackDepth = 0;
	bool bDrainingCommands = false;
	bool bTerminating = false;
	bool bPendingClear = false;
	bool bNotifyingTermination = false;
	void ClearRuntimesImmediate();
	void AddAssetInternal(UStepAsset* InAsset);
	void EndStepInternal(bool bRestoreSteps);
	void Defer(EStepDeferredAction Action, UStepAsset* Asset, const FString& GUID = FString(), int32 Index = INDEX_NONE, bool bOption = false);
	void DrainDeferred();
	bool bLoadingSteps = false;
	bool bStoppingSteps = false;
	bool bClearingRuntimes = false;
	bool bRestoringSteps = false;
	TSet<FString> Transitioning;
	void ClearRuntimes();
	void RestoreTree(UStepBase* InStep);
	void StopTree(UStepBase* InStep);
	bool CanSkipTree(const UStepBase* InStep) const;
	void RefreshAutomaticTimers();
};
