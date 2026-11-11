// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UInputBindingBase;
class UInputComponentBase;
class UInputMappingContext;
class UInputActionBase;
class UEnhancedInputComponent;
enum class ECommonInputType : uint8;
struct FUIInputConfig;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInputTypeChanged, APlayerController*, ECommonInputType);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputRuntimeChanged, ULocalPlayer*);

UCLASS()
class WHFRAMEWORK_API UInputModule : public UModuleBase
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
	virtual void OnControlRefresh(float RealDeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual void RefreshData();

	virtual FParameter ToData() override;

	virtual FParameter GetData() override;

protected:
	FInputModuleSaveData LocalSaveData;
	bool bInputProfilesLoaded = false;
	FName InputContainerProfile;

	void BuildInputCaches();

	void BuildPlayerRuntimes();
	void SynchronizePlayerRuntimes();
	void RemovePlayerRuntime(int32 InIndex);
	void RefreshPlayerControllerBindings();

public:
	// Binding a new profile explicitly adopts the current guest settings when no profile exists.
	// Once bound, a LocalPlayer must be recreated to change identities.
	UFUNCTION(BlueprintCallable, Category = "Input|Profile")
	bool BindLocalInputProfile(APlayerController* InPlayer, const FString& InProfileId);

	UFUNCTION(BlueprintPure, Category = "Input|Profile")
	bool HasPersistentInputProfile(APlayerController* InPlayer) const;

	void ApplyLocalInputProfile(ULocalPlayer* InPlayer, bool bAdoptGuest = false);
	bool CaptureLocalInputProfile(ULocalPlayer* InPlayer);
	void InitializeEmptyLocalProfiles();
	bool PrepareLocalProfileLoad(FName InProfileName);
	bool CanSaveLocalProfile(FName InProfileName) const { return bInputProfilesLoaded && InputContainerProfile == InProfileName; }
	bool AreLocalProfilesApplied(FName InProfileName) const;
	bool TryBindPrimaryInputProfile(APlayerController* InPlayer);

protected:

	void HandleInputMethodChanged(ECommonInputType InInputType, int32 InPlayerIndex);

	void HandleCommonUIInputConfigChanged(FUIInputConfig InInputConfig, int32 InPlayerIndex);

	void HandleGlobalInputModeChanged(APlayerController* InController, EInputMode InPreviousInputMode, EInputMode InInputMode);

	void ApplyCommonUIInputConfig(const FUIInputConfig* InInputConfig, int32 InPlayerIndex);

	void RefreshCommonUIInputMode(int32 InPlayerIndex);

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// InputModule
protected:
	UPROPERTY(EditAnywhere, Category = "Mode")
	EInputMode DefaultInputMode;

	UPROPERTY(EditAnywhere, Instanced, Category = "Binding")
	TArray<TObjectPtr<UInputBindingBase>> InputBindings;

public:
	UFUNCTION(BlueprintPure)
	EInputMode GetDefaultInputMode() const { return DefaultInputMode; }

	UFUNCTION(BlueprintCallable)
	void SetDefaultInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintCallable)
	bool SetPlayerDefaultInputMode(APlayerController* InController, EInputMode InInputMode);

	UFUNCTION(BlueprintCallable)
	bool ResetPlayerDefaultInputMode(APlayerController* InController);

	/** Reset only the input bindings owned by this local controller. */
	UFUNCTION(BlueprintCallable, Category = "InputModule")
	bool ResetPlayerBindings(APlayerController* InController);

	/** Resolve an explicit local controller to this World's runtime slot. */
	int32 FindPlayerRuntimeIndex(APlayerController* InController) const;

	template<class T>
	T* GetInputBinding(int32 InPlayerIndex) const
	{
		return Cast<T>(GetInputBinding(T::StaticClass(), InPlayerIndex));
	}

	UInputBindingBase* GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex) const;

	UInputBindingBase* GetInputBindingByName(const FName InName, int32 InPlayerIndex, TSubclassOf<UInputBindingBase> InClass = nullptr) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Context")
	TArray<FInputContextConfig> ContextConfigs;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputActionBase>> InputActionMap;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UInputMappingContext>> InputContextMap;

	TMultiMap<FGameplayTag, FInputMappableEntry> PlayerMappableByActionTag;

	UPROPERTY(Transient)
	TArray<FInputPlayerRuntime> PlayerRuntimes;
	bool bPlayerRuntimesReady = false;
	bool bInputRuntimeClosing = false;
	bool bSynchronizingPlayerRuntimes = false;

	TMap<TWeakObjectPtr<ULocalPlayer>, EInputMode> PlayerDefaultInputModes;
	TMap<FName, TSet<FString>> FailedInputProfileCaptures;

public:
	const TArray<FInputContextConfig>& GetContextConfigs() const { return ContextConfigs; }

	bool ActivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex);

	bool DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex);

	bool IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex) const;

	TArray<FEnhancedActionKeyMapping> GetAllActionKeyMappings(int32 InPlayerIndex);

	TArray<FName> GetAllActionKeyMappingNames(int32 InPlayerIndex);

	TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex);

	bool IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex) const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	const UInputActionBase* GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured = true) const;

	bool IsInputActionActive(const FGameplayTag& InTag, int32 InPlayerIndex) const;

	bool MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason = nullptr);

	bool MapPlayerKeyByMappingName(FName InMappingName, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason = nullptr, const UObject* SettingOwner = nullptr);

	bool ResetPlayerKeyByTag(FGameplayTag InActionTag, int32 InPlayerIndex);

	bool ResetPlayerKeyByMappingName(FName InMappingName, int32 InPlayerIndex);

	TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByTag(FGameplayTag InActionTag, int32 InPlayerIndex) const;

	TArray<FKey> GetKeysByActionTag(FGameplayTag InActionTag, int32 InPlayerIndex) const;

	TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByMappingName(FName InMappingName, int32 InPlayerIndex) const;

	UFUNCTION(BlueprintPure)
	TArray<FGameplayTag> GetAllMappableActions() const;

	UFUNCTION(BlueprintPure)
	TArray<FInputMappableEntry> GetAllMappableEntries() const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"))
	TArray<FInputMappableEntry> GetMappableEntriesByActionTag(FGameplayTag InActionTag) const;

	ECommonInputType GetCurrentInputType(int32 InPlayerIndex) const;

	bool IsUsingGamepad(int32 InPlayerIndex) const;

	bool IsUsingMouseAndKeyboard(int32 InPlayerIndex) const;

	bool IsUsingTouch(int32 InPlayerIndex) const;

	FOnInputTypeChanged OnInputTypeChanged;
	FOnInputRuntimeChanged OnPlayerRuntimeChanged;
};
