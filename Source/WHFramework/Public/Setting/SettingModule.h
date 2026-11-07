// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "Setting/SettingPresentationTypes.h"
#include "Setting/SettingDefinitionTypes.h"
#include "Setting/SettingEditTypes.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingModule.generated.h"

class USettingProviderBase;
class USettingEntry;
class UWHLocalPlayer;

UCLASS()
class WHFRAMEWORK_API USettingModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(USettingModule)

public:	
	// ParamSets default values for this actor's properties
	USettingModule();

	~USettingModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;
	virtual void OnControlRefresh(float RealDeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void PrepareForHostTermination() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual bool WasLoadDataApplied() const override { return !bLocalProfilePhaseInProgress || bLocalProfilePhaseComplete; }

	virtual void UnloadData(EPhase InPhase) override;

	virtual FParameter ToData() override;

protected:
	void EnsureBuiltinProviders();

	void BuildSettingDefinitions();

	void BuildSettingEntries();

	void CollectPropertyDefinitions(const UStruct* InStruct, const FString& InPrefix, TArray<FSettingDefinition>& OutDefinitions) const;

	bool GetCurrentCombinedSettings(FSettingModuleSaveData& OutData) const;

	FSettingModuleSaveData GetDefaultCombinedSettings() const;

	bool ApplyCombinedSettings(FSettingModuleSaveData& InData);
	bool RestorePendingProviders();
	bool FinalizeHostTermination();
	void FinalizeInterruptedEdit();

	bool SaveSettings();

public:
	bool RetryHostTerminationRecovery();
	bool HasUncommittedAppliedSettings() const { return !AppliedUncommittedSettings.IsEmpty(); }
	bool PrepareLocalProfileLoad(FName InProfileName);
	virtual bool IsLocalProfileDataSupported(const FParameter& InData) const;
	bool LoadLocalProfilePhase(const FParameter& InData, EPhase InPhase);
	void AbortLocalProfileLoad();
	FName GetActiveLocalProfile() const { return ActiveLocalProfile; }
	bool CanSaveLocalProfile(FName InProfileName) const;

protected:
	void MarkLocalProfilePhaseComplete();

	const FSettingDefinition* FindSettingDefinition(FSettingId InSettingId) const;

	USettingProviderBase* FindSettingProvider(const FSettingDefinition& InDefinition) const;

	FSettingValidationResult ValidateSettingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) const;

	bool EvaluateConditions(const TArray<FSettingCondition>& InConditions) const;

	FParameter ReadSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId) const;

	bool WriteSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId, const FParameter& InValue) const;

	void BroadcastEditStateChanged();

protected:
	UPROPERTY(EditAnywhere, Category = "Setting")
	FSettingConfig SettingConfig;

	UPROPERTY(EditAnywhere, Instanced, Category = "Provider")
	TArray<TObjectPtr<USettingProviderBase>> Providers;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Definition")
	TArray<FSettingDefinition> FinalDefinitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USettingEntry>> SettingEntries;

	UPROPERTY(Transient)
	FSettingEditSession EditSession;
	TWeakObjectPtr<APlayerController> EditSessionPlayer;
	TWeakObjectPtr<UWHLocalPlayer> EditSessionLocalPlayer;
	uint64 EditSessionGeneration = 0;
	bool bHostTerminationPending = false;
	bool bFinalizingHostTermination = false;
	bool bLastSettingsSaveSucceeded = false;
	FName ActiveLocalProfile;
	bool bLocalProfilePhaseInProgress = false;
	bool bLocalProfilePhaseComplete = false;
	bool bStartingEdit = false;
	bool bEndingEdit = false;
	bool bEndEditRequested = false;
	bool bCancellingEdit = false;
	bool bApplyingEdit = false;
	TSet<FSettingId> AppliedUncommittedSettings;

	UPROPERTY(Transient)
	FSettingConfirmationTransaction ConfirmationTransaction;

	UPROPERTY(EditAnywhere, Category = "Other", meta = (ClampMin = "1.0"))
	float ConfirmationTimeout = 15.f;

	TMap<FSettingId, TObjectPtr<USettingEntry>> SettingEntryMap;

	TMap<FSettingId, FSettingValidationResult> ValidationResults;

public:
	UPROPERTY(BlueprintAssignable)
	FOnSettingValueChanged OnSettingValueChanged;

	UPROPERTY(BlueprintAssignable)
	FOnSettingEditStateChanged OnSettingEditStateChanged;

	UFUNCTION(BlueprintPure)
	FSettingEditState GetEditState() const;

	UFUNCTION(BlueprintCallable)
	void RegisterProvider(USettingProviderBase* InProvider);

	UFUNCTION(BlueprintCallable)
	void UnregisterProvider(USettingProviderBase* InProvider);

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Setting")
	void RefreshSettingDefinitions();

	UFUNCTION(BlueprintCallable)
	bool BeginEdit(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure, Category = "Setting")
	bool IsEditSessionOwner(APlayerController* InPlayer) const;

	APlayerController* GetEditSessionPlayer() const { return EditSessionPlayer.Get(); }
	bool WasLastSettingsSaveSuccessful() const { return bLastSettingsSaveSucceeded; }

	UFUNCTION(BlueprintCallable)
	bool EndEdit();

	UFUNCTION(BlueprintCallable)
	bool ApplyEditSession();

	UFUNCTION(BlueprintCallable)
	bool CancelEditSession();

	UFUNCTION(BlueprintCallable)
	bool ConfirmPendingSettings();

	UFUNCTION(BlueprintCallable)
	bool RejectPendingSettings();

	UFUNCTION(BlueprintPure)
	bool HasPendingConfirmation() const { return ConfirmationTransaction.bActive; }

	UFUNCTION(BlueprintPure)
	double GetConfirmationTimeRemaining() const;

	UFUNCTION(BlueprintCallable)
	void ResetAllToDefault();

	UFUNCTION(BlueprintPure)
	bool IsEditSessionActive() const { return EditSession.bActive; }

	UFUNCTION(BlueprintPure)
	bool CanApply() const;

	UFUNCTION(BlueprintPure)
	bool CanReset() const;

	UFUNCTION(BlueprintPure)
	bool IsSettingDirty(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	bool CanResetSetting(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	bool IsSettingEnabled(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	bool IsSettingVisible(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	FSettingValidationResult GetValidationResult(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	FParameter GetAppliedValue(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	FParameter GetPendingValue(FSettingId InSettingId) const;

	UFUNCTION(BlueprintPure)
	FParameter GetDefaultValue(FSettingId InSettingId) const;

	UFUNCTION(BlueprintCallable)
	bool SetPendingValue(FSettingId InSettingId, const FParameter& InValue);

	UFUNCTION(BlueprintPure)
	const TArray<FSettingDefinition>& GetSettingDefinitions() const { return FinalDefinitions; }

	UFUNCTION(BlueprintPure)
	TArray<USettingEntry*> GetSettingEntries() const;

	UFUNCTION(BlueprintPure)
	TArray<USettingEntry*> GetSettingEntriesByPage(FName InPage) const;

	UFUNCTION(BlueprintPure)
	virtual TArray<FSettingPageDefinition> GetSettingPages() const;

	UFUNCTION(BlueprintPure)
	float GetConfirmationTimeout() const { return ConfirmationTimeout; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
