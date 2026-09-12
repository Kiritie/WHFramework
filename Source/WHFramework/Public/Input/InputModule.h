// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputManagerInterface.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UInputBindingBase;
class UInputComponentBase;
class UInputMappingContext;
class UInputActionBase;
class UEnhancedInputComponent;
enum class ECommonInputType : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInputTypeChanged, int32, ECommonInputType);

UCLASS()
class WHFRAMEWORK_API UInputModule : public UModuleBase, public IInputManagerInterface
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UInputModule)

public:	
	UInputModule();

	virtual ~UInputModule() override;

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual void RefreshData();

	virtual FSaveData* ToData() override;

	virtual FSaveData* GetData() override;

protected:
	FInputModuleSaveData LocalSaveData;

	void BuildInputCaches();

	void BuildPlayerRuntimes();

	void HandleInputMethodChanged(ECommonInputType InInputType, int32 InPlayerIndex);

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// InputManager
protected:
	UPROPERTY(EditAnywhere, Category = "InputSteups|Mode")
	EInputMode NativeInputMode;

	UPROPERTY(EditAnywhere, Instanced, Category = "InputSteups|Manager")
	TArray<UInputBindingBase*> InputBindings;

public:
	UFUNCTION(BlueprintPure)
	virtual int32 GetNativeInputPriority() const override { return 0; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNativeInputMode(EInputMode InInputMode) override;

	template<class T>
	T* GetInputBinding(int32 InPlayerIndex = 0) const
	{
		return Cast<T>(GetInputBinding(T::StaticClass(), InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UInputBindingBase* GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UInputBindingBase* GetInputBindingByName(const FName InName, int32 InPlayerIndex = 0, TSubclassOf<UInputBindingBase> InClass = nullptr) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Context")
	TArray<FInputContextConfig> ContextConfigs;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputActionBase>> InputActionMap;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UInputMappingContext>> InputContextMap;

	TMultiMap<FGameplayTag, FInputMappableEntry> PlayerMappableByActionTag;

	UPROPERTY(Transient)
	TArray<FInputPlayerRuntime> PlayerRuntimes;

public:
	const TArray<FInputContextConfig>& GetContextConfigs() const { return ContextConfigs; }

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InContextTag"))
	bool ActivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InContextTag"))
	bool DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InContextTag"))
	bool IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllActionKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FName> GetAllActionKeyMappingNames(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	bool IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	const UInputActionBase* GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	bool IsInputActionActive(const FGameplayTag& InTag, int32 InPlayerIndex = 0) const;

	bool MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex = 0, FGameplayTagContainer* OutFailureReason = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InActionTag"))
	bool ResetPlayerKeyByTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"))
	TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	TArray<FGameplayTag> GetAllMappableActions() const;

	UFUNCTION(BlueprintPure)
	ECommonInputType GetCurrentInputType(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	bool IsUsingGamepad(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	bool IsUsingMouseAndKeyboard(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	bool IsUsingTouch(int32 InPlayerIndex = 0) const;

	FOnInputTypeChanged OnInputTypeChanged;
};
