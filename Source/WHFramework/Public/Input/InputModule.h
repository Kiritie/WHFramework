// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UInputManagerBase;
class UInputComponentBase;
class UInputMappingContext;
class UInputActionBase;
class UEnhancedInputComponent;

UCLASS()
class WHFRAMEWORK_API UInputModule : public UModuleBase, public FInputManager
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UInputModule)

public:	
	UInputModule();

	virtual ~UInputModule() override;

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds) override;

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

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Input Manager
protected:
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UInputManagerBase>> InputManagers;

	UPROPERTY(Transient)
	TMap<FName, UInputManagerBase*> InputManagerRefs;

public:
	template<class T>
	T* GetInputManager() const
	{
		return Cast<T>(GetInputManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UInputManagerBase* GetInputManager(TSubclassOf<UInputManagerBase> InClass) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UInputManagerBase* GetInputManagerByName(const FName InName, TSubclassOf<UInputManagerBase> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	void AddKeyShortcut(const FGameplayTag& InTag, const FInputKeyShortcut& InKeyShortcut = FInputKeyShortcut());

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	void RemoveKeyShortcut(const FGameplayTag& InTag);

protected:
	UPROPERTY(EditAnywhere, Category = "InputSteups|Key")
	TMap<FGameplayTag, FInputKeyShortcut> KeyShortcuts;

public:
	UFUNCTION(BlueprintPure)
	TMap<FGameplayTag, FInputKeyShortcut>& GetKeyShortcuts() { return KeyShortcuts; }

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	FInputKeyShortcut GetKeyShortcut(const FGameplayTag& InTag) const;

	//////////////////////////////////////////////////////////////////////////
	// InputMappings
public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	void AddKeyMapping(const FGameplayTag& InTag, const FInputKeyMapping& InKeyMapping);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	void RemoveKeyMapping(const FGameplayTag& InTag);

	UFUNCTION(BlueprintCallable)
	void AddTouchMapping(const FInputTouchMapping& InTouchMapping);
	
	UFUNCTION(BlueprintCallable)
	void AddPlayerKeyMapping(const FName InName, const FKey InKey, int32 InSlot = 0, int32 InPlayerIndex = 0);

protected:
	UFUNCTION(BlueprintCallable)
	void ApplyKeyMappings();

	UFUNCTION(BlueprintCallable)
	void ApplyTouchMappings();

protected:
	UPROPERTY(EditAnywhere, Category = "InputSteups|Context")
	TArray<FInputContextMapping> ContextMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Key")
	TMap<FGameplayTag, FInputKeyMapping> KeyMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Touch")
	TArray<FInputTouchMapping> TouchMappings;

public:
	TArray<FInputContextMapping>& GetContextMappings() { return ContextMappings; }

	UFUNCTION(BlueprintPure)
	TMap<FGameplayTag, FInputKeyMapping>& GetKeyMappings() { return KeyMappings; }

	UFUNCTION(BlueprintPure)
	TArray<FInputTouchMapping>& GetTouchMappings() { return TouchMappings; }

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllActionKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FName> GetAllActionKeyMappingNames(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByName(const FName InName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	FPlayerKeyMappingInfo GetPlayerKeyMappingInfoByName(const FName InName, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	bool IsPlayerMappedKeyByName(const FName InName, const FKey& InKey, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	UInputActionBase* GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	/// TouchInputs
protected:
	UPROPERTY(EditAnywhere, Category = "InputSteups|Touch")
	float TouchInputRate;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION(BlueprintNativeEvent)
	void TouchPressedImpl();

	UFUNCTION(BlueprintNativeEvent)
	void TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION(BlueprintNativeEvent)
	void TouchReleasedImpl(ETouchIndex::Type InTouchIndex);

	UFUNCTION(BlueprintNativeEvent)
	void TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation);

	//////////////////////////////////////////////////////////////////////////
	/// InputStates
protected:
	UPROPERTY(VisibleAnywhere, Category = "InputStates|Touch")
	int32 TouchPressedCount;

	FVector2D TouchLocationPrevious;
	float TouchPinchValuePrevious;
	FTimerHandle TouchReleaseTimerHandle1;
	FTimerHandle TouchReleaseTimerHandle2;
	FTimerHandle TouchReleaseTimerHandle3;

public:
	UFUNCTION(BlueprintPure)
	int32 GetTouchPressedCount() const { return TouchPressedCount; }

	//////////////////////////////////////////////////////////////////////////
	// PlayerController
protected:
	UPROPERTY(Transient)
	AWHPlayerController* PlayerController;
	
protected:
	UFUNCTION(BlueprintPure)
	AWHPlayerController* GetPlayerController();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintCallable)
	virtual void UpdateInputMode() override;

protected:
	UPROPERTY(EditAnywhere, Category = "InputMode")
	EInputMode NativeInputMode;

	UPROPERTY(VisibleAnywhere, Category = "InputMode")
	EInputMode GlobalInputMode;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual bool SetGlobalInputMode(EInputMode InInputMode) override;

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetGlobalInputMode() const override { return GlobalInputMode; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetNativeInputPriority() const override { return 0; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};
