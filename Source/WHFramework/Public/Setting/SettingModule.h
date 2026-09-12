// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingModule.generated.h"

class UWidgetOptionSettingItemBase;
class UWidgetSettingItemCategoryBase;
class UWidgetKeySettingItemBase;
class UWidgetTextSettingItemBase;
class UWidgetEnumSettingItemBase;
class UWidgetBoolSettingItemBase;
class UWidgetFloatSettingItemBase;
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
	void BuildSettingDefinitions();

	void BuildSettingEntries();

	void CollectPropertyDefinitions(const UStruct* InStruct, const FString& InPrefix, TArray<FSettingDefinition>& OutDefinitions) const;

	bool IsSupportedSettingProperty(const FProperty* InProperty) const;

	ESettingRendererType InferRenderer(const FProperty* InProperty) const;

	bool ResolvePropertyPath(UStruct* InRootStruct, void* InRootData, const FString& InPath, FResolvedSettingProperty& OutResolved) const;

	FParameter ReadPropertyValue(const FResolvedSettingProperty& InResolved) const;

	bool WritePropertyValue(const FResolvedSettingProperty& InResolved, const FParameter& InValue) const;

	FSettingModuleSaveData GetCurrentCombinedSettings() const;

	void ApplyCombinedSettings(FSettingModuleSaveData& InData);

	const FSettingDefinition* FindSettingDefinition(FSettingId InSettingId) const;

	FParameter ReadSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId) const;

	bool WriteSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId, const FParameter& InValue) const;

	UPROPERTY(EditAnywhere, Category = "Setting")
	TObjectPtr<USettingRegistry> Registry;

	UPROPERTY(EditAnywhere, Instanced, Category = "Setting")
	TArray<TObjectPtr<USettingProviderBase>> Providers;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Setting")
	TArray<FSettingDefinition> FinalDefinitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USettingEntry>> SettingEntries;

	UPROPERTY(Transient)
	FSettingEditSession EditSession;

	TMap<FSettingId, TObjectPtr<USettingEntry>> SettingEntryMap;

	//////////////////////////////////////////////////////////////////////////
	/// Legacy renderer classes
	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetSettingItemCategoryBase> SettingItemCategoryClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetFloatSettingItemBase> FloatSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetBoolSettingItemBase> BoolSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetEnumSettingItemBase> EnumSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetTextSettingItemBase> TextSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetKeySettingItemBase> KeySettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetOptionSettingItemBase> OptionSettingItemClass;

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Setting")
	void RefreshSettingDefinitions();

	UFUNCTION(BlueprintCallable)
	void BeginEdit();

	UFUNCTION(BlueprintCallable)
	bool ApplyEditSession();

	UFUNCTION(BlueprintCallable)
	void CancelEditSession();

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
	TSubclassOf<UWidgetSettingItemCategoryBase> GetSettingItemCategoryClass() const { return SettingItemCategoryClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetFloatSettingItemBase> GetFloatSettingItemClass() const { return FloatSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetBoolSettingItemBase> GetBoolSettingItemClass() const { return BoolSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetEnumSettingItemBase> GetEnumSettingItemClass() const { return EnumSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetTextSettingItemBase> GetTextSettingItemClass() const { return TextSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetKeySettingItemBase> GetKeySettingItemClass() const { return KeySettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetOptionSettingItemBase> GetOptionSettingItemClass() const { return OptionSettingItemClass; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
