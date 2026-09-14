// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingModule.generated.h"

class USettingRegistry;
class USettingProviderBase;
class USettingEntry;
class FProperty;

struct FResolvedSettingProperty
{
	void* ContainerPtr = nullptr;
	FProperty* Property = nullptr;
};

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

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	void EnsureBuiltinProviders();

	void BuildSettingDefinitions(bool bUpdateSnapshot = false);

	void BuildSettingEntries();

	void CollectPropertyDefinitions(const UStruct* InStruct, const FString& InPrefix, TArray<FSettingDefinition>& OutDefinitions) const;

	bool IsSupportedSettingProperty(const FProperty* InProperty) const;

	ESettingRendererType InferRenderer(const FProperty* InProperty) const;

	bool ResolvePropertyPath(UStruct* InRootStruct, void* InRootData, const FString& InPath, FResolvedSettingProperty& OutResolved) const;

	FParameter ReadPropertyValue(const FResolvedSettingProperty& InResolved) const;

	bool WritePropertyValue(const FResolvedSettingProperty& InResolved, const FParameter& InValue) const;

	FSettingModuleSaveData GetCurrentCombinedSettings() const;

	FSettingModuleSaveData GetDefaultCombinedSettings() const;

	void ApplyCombinedSettings(FSettingModuleSaveData& InData);

	void SaveSettings();

	const FSettingDefinition* FindSettingDefinition(FSettingId InSettingId) const;

	USettingProviderBase* FindSettingProvider(const FSettingDefinition& InDefinition) const;

	FSettingValidationResult ValidateSettingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) const;

	bool EvaluateConditions(const TArray<FSettingCondition>& InConditions) const;

	FParameter ReadSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId) const;

	bool WriteSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId, const FParameter& InValue) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Registry")
	TObjectPtr<USettingRegistry> Registry;

	UPROPERTY(EditAnywhere, Instanced, Category = "Provider")
	TArray<TObjectPtr<USettingProviderBase>> Providers;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Definition")
	TArray<FSettingDefinition> FinalDefinitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USettingEntry>> SettingEntries;

	UPROPERTY(Transient)
	FSettingEditSession EditSession;

	UPROPERTY(Transient)
	FSettingConfirmationTransaction ConfirmationTransaction;

	UPROPERTY(EditAnywhere, Category = "Other", meta = (ClampMin = "1.0"))
	float ConfirmationTimeout = 15.f;

	TMap<FSettingId, TObjectPtr<USettingEntry>> SettingEntryMap;

	TMap<FSettingId, FSettingValidationResult> ValidationResults;

public:
	UPROPERTY(BlueprintAssignable)
	FOnSettingValueChanged OnSettingValueChanged;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Setting")
	void RefreshSettingDefinitions();

#if WITH_EDITOR
	void GenerateRegistrySnapshot(USettingRegistry* InRegistry);
#endif

	UFUNCTION(BlueprintCallable)
	void BeginEdit();

	UFUNCTION(BlueprintCallable)
	void EndEdit();

	UFUNCTION(BlueprintCallable)
	bool ApplyEditSession();

	UFUNCTION(BlueprintCallable)
	void CancelEditSession();

	UFUNCTION(BlueprintCallable)
	bool ConfirmPendingSettings();

	UFUNCTION(BlueprintCallable)
	void RejectPendingSettings();

	UFUNCTION(BlueprintPure)
	bool HasPendingConfirmation() const { return ConfirmationTransaction.bActive; }

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
	TArray<FSettingPageDefinition> GetSettingPages() const;

	UFUNCTION(BlueprintPure)
	float GetConfirmationTimeout() const { return ConfirmationTimeout; }

	UFUNCTION(BlueprintPure)
	USettingRegistry* GetRegistry() const { return Registry; }

	UFUNCTION(BlueprintCallable)
	void SetRegistry(USettingRegistry* InRegistry) { Registry = InRegistry; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
