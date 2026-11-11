// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"
#include "Common/CommonTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"

#include "Algo/Unique.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBindingHandle.h"
#include "Main/Base/ModuleBase.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Input/Event_InputModeChanged.h"
#include "Event/Events/Input/Event_InputTypeChanged.h"
#include "Gameplay/WHPlayerController.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Gameplay/WHGameInstance.h"
#include "Camera/CameraModule.h"
#include "HAL/PlatformMisc.h"
#include "Common/CommonModuleStatics.h"
#include "Main/MainModuleStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Input/InputManager.h"
#include "Input/Base/InputActionBase.h"
#include "Input/InputModuleStatics.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"
#include "Input/Manager/DefaultInputBinding.h"
#include "Input/Manager/InputBindingBase.h"

IMPLEMENTATION_MODULE(UInputModule)

// Sets default values
UInputModule::UInputModule()
{
	ModuleName = FName("InputModule");
	ModuleDisplayName = FText::FromString(TEXT("Input Module"));

	bModuleRequired = true;

	
	DefaultInputMode = EInputMode::GameOnly;
}

UInputModule::~UInputModule()
{
}

#if WITH_EDITOR
void UInputModule::OnGenerate()
{
	Super::OnGenerate();

	if(InputBindings.IsEmpty())
	{
		InputBindings.Add(NewObject<UDefaultInputBinding>(this));
	}

	BuildInputCaches();
}

void UInputModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void UInputModule::OnInitialize()
{
	bInputRuntimeClosing = false;
	Super::OnInitialize();

	BuildInputCaches();

	FInputManager::Get().OnInputModeChanged.AddUObject(this, &ThisClass::HandleGlobalInputModeChanged);

}

void UInputModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BuildPlayerRuntimes();
		FInputManager::Get().UpdateInputMode();
	}
}

void UInputModule::OnReset()
{
	if (!IsBusinessAdmissionOpen()) return;
	Super::OnReset();
	for (const FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		for (UInputBindingBase* Binding : Runtime.InputBindings)
		{
			if (Binding) Binding->OnReset();
		}
	}
}

bool UInputModule::ResetPlayerBindings(APlayerController* InController)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!::IsValid(InController) || InController->GetWorld() != GetWorld() || !InController->IsLocalController()) return false;
	for (const FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		if (Runtime.PlayerController.Get() != InController) continue;
		for (UInputBindingBase* Binding : Runtime.InputBindings)
		{
			if (Binding) Binding->OnReset();
		}
		return true;
	}
	return false;
}

int32 UInputModule::FindPlayerRuntimeIndex(APlayerController* InController) const
{
	if (!::IsValid(InController) || InController->GetWorld() != GetWorld() || !InController->IsLocalController()) return INDEX_NONE;
	return PlayerRuntimes.IndexOfByPredicate([InController](const FInputPlayerRuntime& Runtime)
	{
		return Runtime.PlayerController.Get() == InController && Runtime.LocalPlayer == InController->GetLocalPlayer();
	});
}

void UInputModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	if (!IsBusinessAdmissionOpen()) return;
	Super::OnRefresh(DeltaSeconds, bInEditor);
	if (bInEditor || !bPlayerRuntimesReady) return;
	for (const FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		for (UInputBindingBase* Binding : Runtime.InputBindings)
		{
			if (Binding) Binding->OnRefresh(DeltaSeconds);
		}
	}
}

void UInputModule::OnControlRefresh(float RealDeltaSeconds)
{
	if (!IsBusinessAdmissionOpen()) return;
	Super::OnControlRefresh(RealDeltaSeconds);
	if (!bPlayerRuntimesReady) return;
	SynchronizePlayerRuntimes();
	if (bPlayerRuntimesReady) RefreshPlayerControllerBindings();
}

void UInputModule::OnPause()
{
	Super::OnPause();

	for(const FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		if(APlayerController* PlayerController = Runtime.LocalPlayer
			? Runtime.LocalPlayer->GetPlayerController(GetWorld())
			: nullptr)
		{
			PlayerController->DisableInput(nullptr);
		}
	}
}

void UInputModule::OnUnPause()
{
	Super::OnUnPause();

	for(const FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		if(APlayerController* PlayerController = Runtime.LocalPlayer
			? Runtime.LocalPlayer->GetPlayerController(GetWorld())
			: nullptr)
		{
			PlayerController->EnableInput(nullptr);
		}
	}
}

void UInputModule::OnTermination(EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Final))
	{
		bInputRuntimeClosing = true;
		bPlayerRuntimesReady = false;
	}
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		FInputManager::Get().OnInputModeChanged.RemoveAll(this);


		for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index) RemovePlayerRuntime(Index);
		PlayerRuntimes.Reset();
		PlayerDefaultInputModes.Reset();
	}
}

void UInputModule::BuildInputCaches()
{
	InputActionMap.Reset();
	InputContextMap.Reset();
	PlayerMappableByActionTag.Reset();

	TMap<FName, int32> NextSlotByMappingName;
	TMap<FString, FGameplayTag> ActionTagByMappingSlot;
	auto AddMappings = [this, &NextSlotByMappingName, &ActionTagByMappingSlot](
		UInputMappingContext* MappingContext,
		bool bRegisterWithSettings)
	{
		if(!MappingContext)
		{
			return;
		}

		for(const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			const UInputActionBase* Action = Cast<UInputActionBase>(Mapping.Action);
			if(!ensureMsgf(
				Action,
				TEXT("Input mapping context %s contains an action that is not UInputActionBase."),
				*GetNameSafe(MappingContext)))
			{
				continue;
			}
			if(!ensureMsgf(
				Action->ActionTag.IsValid(),
				TEXT("Input action %s has no valid ActionTag."),
				*GetNameSafe(Action)))
			{
				continue;
			}

			const TObjectPtr<const UInputActionBase>* Existing = InputActionMap.Find(Action->ActionTag);
			if(!ensureMsgf(!Existing || Existing->Get() == Action, TEXT("Input ActionTag collision: %s"), *Action->ActionTag.ToString()))
			{
				continue;
			}
			if(!Existing)
			{
				InputActionMap.Add(Action->ActionTag, Action);
			}

			if(!bRegisterWithSettings || !Mapping.IsPlayerMappable())
			{
				continue;
			}
			const FName MappingName = Mapping.GetMappingName();
			if(!ensureMsgf(!MappingName.IsNone(), TEXT("Player mappable action %s has no MappingName."), *Action->ActionTag.ToString()))
			{
				continue;
			}

			const int32 SlotIndex = NextSlotByMappingName.FindOrAdd(MappingName)++;
			if(!ensureMsgf(
				SlotIndex <= static_cast<int32>(EPlayerMappableKeySlot::Seventh),
				TEXT("Player mappable row %s exceeds the supported slot count."),
				*MappingName.ToString()))
			{
				continue;
			}

			const EPlayerMappableKeySlot Slot = static_cast<EPlayerMappableKeySlot>(SlotIndex);
			const FString MappingSlotKey = FString::Printf(TEXT("%s.%d"), *MappingName.ToString(), SlotIndex);
			if(const FGameplayTag* ExistingActionTag = ActionTagByMappingSlot.Find(MappingSlotKey))
			{
				if(!ensureMsgf(
					*ExistingActionTag == Action->ActionTag,
					TEXT("Player mappable MappingName/Slot collision: %s"),
					*MappingSlotKey))
				{
					continue;
				}
			}
			ActionTagByMappingSlot.Add(MappingSlotKey, Action->ActionTag);

			FInputMappableEntry Entry;
			Entry.ActionTag = Action->ActionTag;
			Entry.MappingName = MappingName;
			Entry.Slot = Slot;
			Entry.DefaultKey = Mapping.Key;
			Entry.DisplayName = Mapping.GetDisplayName();
			Entry.DisplayCategory = Mapping.GetDisplayCategory();
			PlayerMappableByActionTag.Add(Action->ActionTag, Entry);
		}
	};

	for(const FInputContextConfig& Config : ContextConfigs)
	{
		if(!ensureMsgf(
			Config.ContextTag.IsValid(),
			TEXT("Input context config has no valid ContextTag.")))
		{
			continue;
		}
		if(!ensureMsgf(
			Config.MappingContext,
			TEXT("Input context %s has no mapping context."),
			*Config.ContextTag.ToString()))
		{
			continue;
		}

		if(!ensureMsgf(!InputContextMap.Contains(Config.ContextTag), TEXT("Input ContextTag collision: %s"), *Config.ContextTag.ToString()))
		{
			continue;
		}
		InputContextMap.Add(Config.ContextTag, Config.MappingContext);
		AddMappings(Config.MappingContext, Config.bRegisterWithSettings);
	}

}

void UInputModule::BuildPlayerRuntimes()
{
	if (!IsBusinessAdmissionOpen() || bInputRuntimeClosing) return;
    bPlayerRuntimesReady = false;
    for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index)
    {
        RemovePlayerRuntime(Index);
		if (!IsBusinessAdmissionOpen() || bInputRuntimeClosing) return;
    }
    PlayerRuntimes.Reset();
    bPlayerRuntimesReady = true;
    SynchronizePlayerRuntimes();
    if (bPlayerRuntimesReady) RefreshPlayerControllerBindings();
}

void UInputModule::RemovePlayerRuntime(int32 InIndex)
{
    if (!PlayerRuntimes.IsValidIndex(InIndex) || !PlayerRuntimes[InIndex].LocalPlayer) return;
    UWorld* World = GetWorld();
    FInputPlayerRuntime Previous = MoveTemp(PlayerRuntimes[InIndex]);
    PlayerRuntimes[InIndex] = FInputPlayerRuntime();
    ULocalPlayer* LocalPlayer = Previous.LocalPlayer.Get();
    if (Previous.CommonInputSubsystem) Previous.CommonInputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
    if (Previous.CommonUIActionRouter)
    {
        Previous.CommonUIActionRouter->OnActiveInputConfigChanged().RemoveAll(this);
        Previous.CommonUIActionRouter->OnBoundActionsUpdated().RemoveAll(this);
    }
    PlayerDefaultInputModes.Remove(TWeakObjectPtr<ULocalPlayer>(LocalPlayer));
    CaptureLocalInputProfile(LocalPlayer);
    if (UCameraModule* Camera = UCameraModule::GetPtr(World)) Camera->CaptureLocalCameraProfile(LocalPlayer);
    FInputManager::Get().ReleasePlayerInputMode(Previous.PlayerController.Get());
    for (UInputBindingBase* Binding : Previous.InputBindings) if (Binding) Binding->OnTermination();
    if (UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(LocalPlayer)) Player->ReleaseWorldInputContexts(World);
    if (IsBusinessAdmissionOpen()) OnPlayerRuntimeChanged.Broadcast(LocalPlayer);
}

void UInputModule::SynchronizePlayerRuntimes()
{
	if (!IsBusinessAdmissionOpen()) return;
    if (!bPlayerRuntimesReady || bSynchronizingPlayerRuntimes) return;
    TGuardValue<bool> Guard(bSynchronizingPlayerRuntimes, true);
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    if (!World || World->bIsTearingDown || !Instance) return;
    const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
    for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index)
    {
        if (PlayerRuntimes[Index].LocalPlayer && !Players.Contains(PlayerRuntimes[Index].LocalPlayer.Get()))
        {
            RemovePlayerRuntime(Index);
            if (!IsBusinessAdmissionOpen() || !bPlayerRuntimesReady) return;
        }
    }
    for (ULocalPlayer* Player : Players)
    {
        if (!::IsValid(Player) || PlayerRuntimes.ContainsByPredicate([Player](const FInputPlayerRuntime& Runtime) { return Runtime.LocalPlayer == Player; })) continue;
        int32 Index = PlayerRuntimes.IndexOfByPredicate([](const FInputPlayerRuntime& Runtime) { return !Runtime.LocalPlayer; });
        if (Index == INDEX_NONE) Index = PlayerRuntimes.AddDefaulted();
        FInputPlayerRuntime& Runtime = PlayerRuntimes[Index];
        Runtime.LocalPlayer = Player;
        Runtime.EnhancedInputSubsystem = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        Runtime.CommonInputSubsystem = Player->GetSubsystem<UCommonInputSubsystem>();
        Runtime.CommonUIActionRouter = Player->GetSubsystem<UCommonUIActionRouterBase>();
        Runtime.UserSettings = Runtime.EnhancedInputSubsystem ? Runtime.EnhancedInputSubsystem->GetUserSettings() : nullptr;
        Runtime.PlayerController = Player->GetPlayerController(World);
        Runtime.InputComponent = Runtime.PlayerController.IsValid() ? Cast<UInputComponentBase>(Runtime.PlayerController->InputComponent) : nullptr;
        if (Runtime.UserSettings)
            for (const FInputContextConfig& Config : ContextConfigs)
            {
                if (Config.bRegisterWithSettings && Config.MappingContext) PlayerRuntimes[Index].UserSettings->RegisterInputMappingContext(Config.MappingContext);
            }
        ApplyLocalInputProfile(Player);
        if (UCameraModule* Camera = UCameraModule::GetPtr(World)) Camera->ApplyLocalCameraProfile(Player);
        TryBindPrimaryInputProfile(PlayerRuntimes[Index].PlayerController.Get());
        if (PlayerRuntimes[Index].CommonInputSubsystem) PlayerRuntimes[Index].CommonInputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged, Index);
        if (PlayerRuntimes[Index].CommonUIActionRouter)
        {
            PlayerRuntimes[Index].CommonUIActionRouter->OnActiveInputConfigChanged().AddUObject(this, &ThisClass::HandleCommonUIInputConfigChanged, Index);
            PlayerRuntimes[Index].CommonUIActionRouter->OnBoundActionsUpdated().AddUObject(this, &ThisClass::RefreshCommonUIInputMode, Index);
        }
        const TArray<TObjectPtr<UInputBindingBase>> Templates = InputBindings;
        for (UInputBindingBase* Template : Templates)
        {
            if (!Template) continue;
            UInputBindingBase* Binding = DuplicateObject<UInputBindingBase>(Template, this);
            if (!Binding) continue;
            PlayerRuntimes[Index].InputBindings.Add(Binding);
            Binding->OnInitialize(Player);
            if (UInputComponentBase* Component = PlayerRuntimes[Index].InputComponent)
            {
                Binding->OnBindInput(Component);
            }
        }
        for (const FInputContextConfig& Config : ContextConfigs)
        {
            if (Config.bAutoActivate) ActivateInputContext(Config.ContextTag, Index);
        }
        RefreshCommonUIInputMode(Index);
        OnPlayerRuntimeChanged.Broadcast(Player);
    }
}

void UInputModule::RefreshPlayerControllerBindings()
{
	if (!IsBusinessAdmissionOpen()) return;
    for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index)
    {
        ULocalPlayer* LocalPlayer = PlayerRuntimes[Index].LocalPlayer;
        if (!LocalPlayer) continue;
        APlayerController* Controller = LocalPlayer->GetPlayerController(GetWorld());
        UInputComponentBase* Component = Controller ? Cast<UInputComponentBase>(Controller->InputComponent) : nullptr;
        const TWeakObjectPtr<APlayerController> PreviousController = PlayerRuntimes[Index].PlayerController;
        const bool bControllerChanged = PlayerRuntimes[Index].PlayerController.Get() != Controller;
        const bool bComponentChanged = PlayerRuntimes[Index].InputComponent != Component;
        if (bComponentChanged)
        {
            TArray<TWeakObjectPtr<UInputBindingBase>> Bindings;
            for (UInputBindingBase* Binding : PlayerRuntimes[Index].InputBindings) if (Binding) Bindings.Add(Binding);
            for (const TWeakObjectPtr<UInputBindingBase>& Binding : Bindings)
            {
                if (Binding.IsValid()) Binding->OnUnbindInput();
            }
            if (bControllerChanged) FInputManager::Get().ReleasePlayerInputMode(PreviousController.Get());
            PlayerRuntimes[Index].InputComponent = Component;
            PlayerRuntimes[Index].PlayerController = Controller;
            for (const TWeakObjectPtr<UInputBindingBase>& Binding : Bindings)
            {
                if (Binding.IsValid() && Component) Binding->OnBindInput(Component);
            }
        }
        else if (bControllerChanged)
        {
            FInputManager::Get().ReleasePlayerInputMode(PreviousController.Get());
            PlayerRuntimes[Index].PlayerController = Controller;
        }
        if (bControllerChanged)
        {
            RefreshCommonUIInputMode(Index);
        }
        if (bControllerChanged || bComponentChanged)
        {
            OnPlayerRuntimeChanged.Broadcast(LocalPlayer);
        }
        if (Controller) TryBindPrimaryInputProfile(Controller);
        if (UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(LocalPlayer)) Player->SynchronizeInputContexts();
    }
}

void UInputModule::HandleInputMethodChanged(ECommonInputType InInputType, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return;
	APlayerController* Player = PlayerRuntimes.IsValidIndex(InPlayerIndex) ? PlayerRuntimes[InPlayerIndex].PlayerController.Get() : nullptr;
	if (!Player || Player->GetWorld() != GetWorld()) return;
	OnInputTypeChanged.Broadcast(Player, InInputType);
	if (!IsBusinessAdmissionOpen()) return;
	UEventModuleStatics::BroadcastEvent<FEventInputTypeChanged>(this, { Player, InInputType });
}

void UInputModule::HandleCommonUIInputConfigChanged(FUIInputConfig InInputConfig, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return;
	}

	const UCommonUIActionRouterBase* ActionRouter = PlayerRuntimes[InPlayerIndex].CommonUIActionRouter;
	ApplyCommonUIInputConfig(ActionRouter && ActionRouter->GetLeafmostActivatableWidget() ? &InInputConfig : nullptr, InPlayerIndex);
}

void UInputModule::HandleGlobalInputModeChanged(APlayerController* InController, EInputMode InPreviousInputMode, EInputMode InInputMode)
{
	if (!IsBusinessAdmissionOpen()) return;
	if (!InController || InController->GetWorld() != GetWorld()) return;
	UEventModuleStatics::BroadcastEvent<FEventInputModeChanged>(this, { InController, InInputMode, InPreviousInputMode });
}

void UInputModule::ApplyCommonUIInputConfig(const FUIInputConfig* InInputConfig, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return;
	TOptional<EInputMode> InputMode;
	if(InInputConfig)
	{
		switch(InInputConfig->GetInputMode())
		{
			case ECommonInputMode::Game:
				InputMode = EInputMode::GameOnly;
				break;
			case ECommonInputMode::All:
				InputMode = EInputMode::GameAndUI;
				break;
			case ECommonInputMode::Menu:
				InputMode = EInputMode::UIOnly;
				break;
			default:
				break;
		}
	}

	const FInputPlayerRuntime& Runtime = PlayerRuntimes[InPlayerIndex];
	const EInputMode* PlayerDefault = PlayerDefaultInputModes.Find(TWeakObjectPtr<ULocalPlayer>(Runtime.LocalPlayer.Get()));
	FInputManager::Get().SetPlayerInputMode(Runtime.PlayerController.Get(), PlayerDefault ? *PlayerDefault : DefaultInputMode, InputMode);
}

void UInputModule::RefreshCommonUIInputMode(int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return;
	}

	UCommonUIActionRouterBase* ActionRouter = PlayerRuntimes[InPlayerIndex].CommonUIActionRouter;
	const UCommonActivatableWidget* ActiveWidget = ActionRouter
		? ActionRouter->GetLeafmostActivatableWidget()
		: nullptr;
	const TOptional<FUIInputConfig> InputConfig = ActiveWidget
		? ActiveWidget->GetDesiredInputConfig()
		: TOptional<FUIInputConfig>();
	if(ActionRouter && InputConfig.IsSet())
	{
		ActionRouter->SetActiveUIInputConfig(InputConfig.GetValue(), ActiveWidget);
		const APlayerController* Controller = PlayerRuntimes[InPlayerIndex].PlayerController.Get();
		const UGameViewportClient* Viewport = Controller ? Controller->GetWorld()->GetGameViewport() : nullptr;
		const EMouseCaptureMode ExpectedCapture = ActionRouter->GetActiveMouseCaptureMode();
		if(Controller && Viewport &&
			(Viewport->GetMouseCaptureMode() != ExpectedCapture ||
				(ExpectedCapture == EMouseCaptureMode::NoCapture && !Controller->bShowMouseCursor)))
		{
			ActionRouter->RefreshUIInputConfig();
		}
	}
	ApplyCommonUIInputConfig(InputConfig.IsSet() ? &InputConfig.GetValue() : nullptr, InPlayerIndex);
}

void UInputModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen())
	{
		MarkLoadDataFailed();
		return;
	}
	if (!PHASEC(InPhase, EPhase::Final)) return;
	if (!InSaveData.Is<FInputModuleSaveData>())
	{
		UE_LOG(LogTemp, Error, TEXT("当前世界的输入档案数据类型无效。"));
		return;
	}
	LocalSaveData = InSaveData.GetRef<FInputModuleSaveData>();
	// The former unowned byte array cannot be attributed to a player.
	LocalSaveData.GetDatas().Reset();
	bInputProfilesLoaded = true;
	TArray<ULocalPlayer*> Players;
	if (const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) if (Player) Players.Emplace(Player);
	for (ULocalPlayer* Player : Players)
	{
		ApplyLocalInputProfile(Player);
	}
	for (ULocalPlayer* Player : Players)
	{
		TryBindPrimaryInputProfile(Player->GetPlayerController(GetWorld()));
	}
}

void UInputModule::UnloadData(EPhase InPhase)
{
	
}

void UInputModule::RefreshData()
{
	if (!IsBusinessAdmissionOpen()) return;
	const FParameter Data = GetSaveData(true);
	if (Data.Is<FInputModuleSaveData>()) LocalSaveData = Data.GetRef<FInputModuleSaveData>();
}

FParameter UInputModule::ToData()
{
	if (!bInputProfilesLoaded || InputContainerProfile.IsNone()) return FParameter();
	TArray<ULocalPlayer*> Players;
	if (const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) if (Player) Players.Emplace(Player);
	for (ULocalPlayer* Entry : Players)
	{
		const UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(Entry);
		const UEnhancedInputLocalPlayerSubsystem* Input = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		if (Local && Local->GetPlayerController(GetWorld()) && Input && Input->GetUserSettings() &&
			(Local->InputContainerProfile != InputContainerProfile || !Local->bInputProfileApplied)) return FParameter();
	}
	for (ULocalPlayer* Player : Players)
	{
		if (!CaptureLocalInputProfile(Player)) return FParameter();
	}
	if (const TSet<FString>* Failures = FailedInputProfileCaptures.Find(InputContainerProfile); Failures && !Failures->IsEmpty()) return FParameter();
	return FParameter(LocalSaveData);
}

FParameter UInputModule::GetData()
{
	return ToData();
}

void UInputModule::InitializeEmptyLocalProfiles()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bInputProfilesLoaded || InputContainerProfile.IsNone()) return;
	LocalSaveData = FInputModuleSaveData();
	bInputProfilesLoaded = true;
	if (UCameraModule* Camera = UCameraModule::GetPtr(GetWorld())) Camera->InitializeEmptyLocalProfiles();
	TArray<ULocalPlayer*> Players;
	if (const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) if (Player) Players.Emplace(Player);
	for (ULocalPlayer* Player : Players)
	{
		ApplyLocalInputProfile(Player);
		if (UCameraModule* Camera = UCameraModule::GetPtr(GetWorld())) Camera->ApplyLocalCameraProfile(Player);
		TryBindPrimaryInputProfile(Player->GetPlayerController(GetWorld()));
	}
}

bool UInputModule::PrepareLocalProfileLoad(FName InProfileName)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (InProfileName.IsNone()) return false;
	const bool bReloadCurrent = bInputProfilesLoaded && InputContainerProfile == InProfileName;
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!Instance) return false;
	const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
	for (ULocalPlayer* Entry : Players)
	{
		if (UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(Entry); Player && (!Player->RetryPendingSettingRecovery() || Player->HasSettingEdit())) return false;
	}
	TArray<FName> OldProfiles;
	TArray<TArray<uint8>> Captures;
	OldProfiles.Reserve(Players.Num());
	Captures.SetNum(Players.Num());
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(Players[Index]);
		OldProfiles.Add(Player ? Player->InputContainerProfile : NAME_None);
		if (!Player || Player->InputContainerProfile == InProfileName || Player->InputContainerProfile.IsNone() || !Player->bInputProfileApplied) continue;
		UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		UEnhancedInputUserSettings* Settings = Input ? Input->GetUserSettings() : nullptr;
		if (!Settings || !UCommonModuleStatics::SaveObjectDataToMemory(Settings, Captures[Index]))
		{
			UE_LOG(LogTemp, Error, TEXT("无法捕获本地玩家的当前输入档案。"));
			return false;
		}
	}
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(Players[Index]);
		if (!Player) continue;
		Player->InvalidateSettingEditForProfileChange();
		if (Player->InputContainerProfile == InProfileName)
		{
			if (bReloadCurrent && !Player->InputProfileId.IsEmpty()) { Player->InputProfileSessions.Remove(InProfileName); Player->bInputProfileApplied = false; }
			continue;
		}
		if (!Captures[Index].IsEmpty()) Player->InputProfileSessions.Add(OldProfiles[Index], MoveTemp(Captures[Index]));
		Player->InputContainerProfile = InProfileName;
		Player->bInputProfileApplied = false;
	}
	InputContainerProfile = InProfileName;
	bInputProfilesLoaded = false;
	return true;
}

bool UInputModule::AreLocalProfilesApplied(FName InProfileName) const
{
	if (!CanSaveLocalProfile(InProfileName)) return false;
	const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!Instance) return false;
	for (const ULocalPlayer* Entry : Instance->GetLocalPlayers())
	{
		const UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(Entry);
		if (!Player || !Player->GetPlayerController(GetWorld())) continue;
		const UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (Input && Input->GetUserSettings() && (Player->InputContainerProfile != InProfileName || !Player->bInputProfileApplied)) return false;
	}
	return true;
}

bool UInputModule::TryBindPrimaryInputProfile(APlayerController* InPlayer)
{
	if (!IsBusinessAdmissionOpen()) return false;
#if PLATFORM_WINDOWS
	if (!bInputProfilesLoaded || !::IsValid(InPlayer) || InPlayer->GetWorld() != GetWorld() || !InPlayer->IsLocalController()) return false;
	UWHLocalPlayer* LocalPlayer = Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer());
	UWHGameInstance* Instance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!LocalPlayer || !LocalPlayer->IsPrimaryPlayer() || !Instance || Instance->HasClaimedPrimaryInputProfile() ||
		!LocalPlayer->InputProfileId.IsEmpty()) return false;
	const FString LoginId = FPlatformMisc::GetLoginId();
	if (LoginId.IsEmpty() || !BindLocalInputProfile(InPlayer, TEXT("OSLogin:") + LoginId)) return false;
	Instance->ClaimPrimaryInputProfile();
	return true;
#else
	return false;
#endif
}

bool UInputModule::BindLocalInputProfile(APlayerController* InPlayer, const FString& InProfileId)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!bInputProfilesLoaded || !::IsValid(InPlayer) || !InPlayer->IsLocalController() ||
		InPlayer->GetWorld() != GetWorld() || InProfileId.IsEmpty() || InProfileId.Len() > 128 ||
		InProfileId != InProfileId.TrimStartAndEnd()) return false;
	UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer());
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!Player || !Instance || Player->HasSettingEdit() || !Player->InputProfileId.IsEmpty()) return false;
	for (const ULocalPlayer* Other : Instance->GetLocalPlayers())
	{
		const UWHLocalPlayer* OtherPlayer = Cast<UWHLocalPlayer>(Other);
		if (OtherPlayer && OtherPlayer != Player && OtherPlayer->InputProfileId == InProfileId) return false;
	}
	UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Input || !Input->GetUserSettings()) return false;
	const auto PreviousInputSessions = Player->InputProfileSessions;
	const auto PreviousCameraSessions = Player->CameraProfileSessions;
	const bool bPreviousInputApplied = Player->bInputProfileApplied;
	const bool bPreviousCameraApplied = Player->bCameraProfileApplied;
	Player->InvalidateSettingEditForProfileChange();
	Player->InputProfileId = InProfileId;
	Player->InputProfileSessions.Reset();
	Player->CameraProfileSessions.Reset();
	Player->bInputProfileApplied = false;
	Player->bCameraProfileApplied = false;
	ApplyLocalInputProfile(Player, true);
	if (!Player->bInputProfileApplied)
	{
		Player->InputProfileId.Reset();
		Player->InputProfileSessions = PreviousInputSessions;
		Player->CameraProfileSessions = PreviousCameraSessions;
		Player->bInputProfileApplied = bPreviousInputApplied;
		Player->bCameraProfileApplied = bPreviousCameraApplied;
		return false;
	}
	if (UCameraModule* Camera = UCameraModule::GetPtr(GetWorld())) Camera->ApplyLocalCameraProfile(Player, true);
	return Player->bInputProfileApplied;
}

bool UInputModule::HasPersistentInputProfile(APlayerController* InPlayer) const
{
	if (!::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld()) return false;
	const UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer());
	return Player && !Player->InputProfileId.IsEmpty() && Player->bInputProfileApplied;
}

void UInputModule::ApplyLocalInputProfile(ULocalPlayer* InPlayer, bool bAdoptGuest)
{
	if (!IsBusinessAdmissionOpen()) return;
	UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(InPlayer);
	if (!bInputProfilesLoaded || !Player || Player->HasSettingEdit() || InputContainerProfile.IsNone()) return;
	if (Player->InputContainerProfile.IsNone()) Player->InputContainerProfile = InputContainerProfile;
	if (Player->InputContainerProfile != InputContainerProfile || Player->bInputProfileApplied || Player->bApplyingInputProfile) return;
	UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedInputUserSettings* Settings = Input ? Input->GetUserSettings() : nullptr;
	if (!Settings) return;
	TGuardValue<bool> Applying(Player->bApplyingInputProfile, true);
	if (Player->DefaultInputSettings.IsEmpty())
	{
		TArray<uint8> Defaults;
		if (!UCommonModuleStatics::SaveObjectDataToMemory(Settings, Defaults)) return;
		Player->DefaultInputSettings = MoveTemp(Defaults);
	}
	const TArray<uint8>* Bytes = Player->InputProfileSessions.Find(InputContainerProfile);
	if (!Bytes && !Player->InputProfileId.IsEmpty())
		if (const FInputLocalProfileData* Profile = LocalSaveData.Profiles.Find(Player->InputProfileId)) Bytes = &Profile->Settings;
	if (!Bytes && !bAdoptGuest) Bytes = &Player->DefaultInputSettings;
	if (Bytes && !Bytes->IsEmpty())
	{
		const TArray<uint8> Data = *Bytes;
		TArray<uint8> Previous;
		if (!UCommonModuleStatics::SaveObjectDataToMemory(Settings, Previous)) return;
		const bool bLoaded = UCommonModuleStatics::LoadObjectDataFromMemory(Settings, Data);
		if (!bLoaded)
		{
			const bool bRestored = UCommonModuleStatics::LoadObjectDataFromMemory(Settings, Previous);
			UE_LOG(LogTemp, Error, TEXT("本地玩家输入档案无效，恢复先前设置%s。"), bRestored ? TEXT("成功") : TEXT("失败"));
			return;
		}
	}
	Player->bInputProfileApplied = true;
}

bool UInputModule::CaptureLocalInputProfile(ULocalPlayer* InPlayer)
{
	UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(InPlayer);
	if (!bInputProfilesLoaded || !Player || Player->InputContainerProfile != InputContainerProfile || Player->InputProfileId.IsEmpty()) return true;
	if (!Player->CanCaptureSettings()) return false;
	const FString ProfileId = Player->InputProfileId;
	const FName Container = InputContainerProfile;
	if (Player->bApplyingInputProfile) return false;
	UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedInputUserSettings* Settings = Input ? Input->GetUserSettings() : nullptr;
	if (!Player->bInputProfileApplied && (!Player->GetPlayerController(GetWorld()) || !Settings)) return true;
	if (!Player->bInputProfileApplied) { FailedInputProfileCaptures.FindOrAdd(Container).Add(ProfileId); return false; }
	if (!Settings) { FailedInputProfileCaptures.FindOrAdd(Container).Add(ProfileId); return false; }
	TArray<uint8> Bytes;
	const bool bCaptured = UCommonModuleStatics::SaveObjectDataToMemory(Settings, Bytes);
	if (!bCaptured)
	{
		FailedInputProfileCaptures.FindOrAdd(Container).Add(ProfileId);
		UE_LOG(LogTemp, Error, TEXT("无法保存本地玩家的输入设置。"));
		return false;
	}
	LocalSaveData.Profiles.FindOrAdd(ProfileId).Settings = MoveTemp(Bytes);
	if (TSet<FString>* Failures = FailedInputProfileCaptures.Find(Container)) Failures->Remove(ProfileId);
	return true;
}

FString UInputModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Local input runtimes: %d"), PlayerRuntimes.Num());
}

void UInputModule::SetDefaultInputMode(EInputMode InInputMode)
{
	if (!IsBusinessAdmissionOpen()) return;
	DefaultInputMode = InInputMode;
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerRuntimes.Num(); ++PlayerIndex) RefreshCommonUIInputMode(PlayerIndex);
}

bool UInputModule::SetPlayerDefaultInputMode(APlayerController* InController, EInputMode InInputMode)
{
	if (!IsBusinessAdmissionOpen()) return false;
    if (!::IsValid(InController) || InController->GetWorld() != GetWorld() || !InController->IsLocalController()) return false;
    for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index)
    {
        FInputPlayerRuntime& Runtime = PlayerRuntimes[Index];
        if (Runtime.PlayerController.Get() != InController || !Runtime.LocalPlayer) continue;
        PlayerDefaultInputModes.Add(TWeakObjectPtr<ULocalPlayer>(Runtime.LocalPlayer.Get()), InInputMode);
        RefreshCommonUIInputMode(Index);
        return true;
    }
    return false;
}

bool UInputModule::ResetPlayerDefaultInputMode(APlayerController* InController)
{
	if (!IsBusinessAdmissionOpen()) return false;
    if (!::IsValid(InController) || InController->GetWorld() != GetWorld()) return false;
    for (int32 Index = 0; Index < PlayerRuntimes.Num(); ++Index)
    {
        FInputPlayerRuntime& Runtime = PlayerRuntimes[Index];
        if (Runtime.PlayerController.Get() != InController || !Runtime.LocalPlayer) continue;
        PlayerDefaultInputModes.Remove(TWeakObjectPtr<ULocalPlayer>(Runtime.LocalPlayer.Get()));
        RefreshCommonUIInputMode(Index);
        return true;
    }
    return false;
}

UInputBindingBase* UInputModule::GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex) const
{
	if(!InClass)
	{
		return nullptr;
	}

	const FName InputBindingName = InClass->GetDefaultObject<UInputBindingBase>()->GetInputBindingName();
	return GetInputBindingByName(InputBindingName, InPlayerIndex, InClass);
}

UInputBindingBase* UInputModule::GetInputBindingByName(const FName InName, int32 InPlayerIndex, TSubclassOf<UInputBindingBase> InClass) const
{
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return nullptr;
	}

	for(UInputBindingBase* InputBinding : PlayerRuntimes[InPlayerIndex].InputBindings)
	{
		if(InputBinding && InputBinding->GetInputBindingName() == InName)
		{
			return GetDeterminesOutputObject(InputBinding, InClass);
		}
	}
	return nullptr;
}

TArray<FEnhancedActionKeyMapping> UInputModule::GetAllActionKeyMappings(int32 InPlayerIndex)
{
	TArray<FEnhancedActionKeyMapping> Mappings;
	for(const FInputContextConfig& Config : ContextConfigs)
	{
		if(!Config.MappingContext) continue;
		for(const FEnhancedActionKeyMapping& Mapping : Config.MappingContext->GetMappings())
		{
			Mappings.Add(Mapping);
		}
	}
	return Mappings;
}

TArray<FName> UInputModule::GetAllActionKeyMappingNames(int32 InPlayerIndex)
{
	TArray<FName> MappingNames;
	for(const FInputContextConfig& Config : ContextConfigs)
	{
		if(!Config.MappingContext) continue;
		for(const FEnhancedActionKeyMapping& Mapping : Config.MappingContext->GetMappings())
		{
			const FName MappingName = Mapping.GetMappingName();
			if(!MappingName.IsNone())
			{
				MappingNames.AddUnique(MappingName);
			}
		}
	}
	return MappingNames;
}

TArray<FPlayerKeyMapping> UInputModule::GetAllPlayerKeyMappings(int32 InPlayerIndex)
{
	TArray<FPlayerKeyMapping> Mappings;
	const UEnhancedInputUserSettings* Settings = PlayerRuntimes.IsValidIndex(InPlayerIndex)
		? PlayerRuntimes[InPlayerIndex].UserSettings.Get()
		: nullptr;
	if(Settings)
	{
		for (auto& Iter1 : Settings->GetAllAvailableKeyProfiles())
		{
			for (const auto& Iter2 : Iter1.Value->GetPlayerMappingRows())
			{
				if (Iter2.Value.HasAnyMappings())
				{
					for (const FPlayerKeyMapping& Mapping : Iter2.Value.Mappings)
					{
						Mappings.Add(Mapping);
					}
				}
			}
		}
	}
	if(Mappings.Num() > 1)
	{
		const TArray<FName> MappingNames = GetAllActionKeyMappingNames(InPlayerIndex);
		Mappings.Sort([MappingNames](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B){
			if(A.GetMappingName() == B.GetMappingName()) return A.GetSlot() < B.GetSlot();
			return MappingNames.IndexOfByKey(A.GetMappingName()) < MappingNames.IndexOfByKey(B.GetMappingName());
		});
	}
	return Mappings;
}

bool UInputModule::IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex) const
{
	for(const FPlayerKeyMapping& Mapping : GetPlayerKeyMappingsByTag(InTag, InPlayerIndex))
	{
		if(InKey == Mapping.GetCurrentKey())
		{
			return true;
		}
	}
	return false;
}

const UInputActionBase* UInputModule::GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured) const
{
	if(const TObjectPtr<const UInputActionBase>* Action = InputActionMap.Find(InTag))
	{
		return Action->Get();
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Can't find InputAction for InputTag [%s] on InputConfig [%s]."), *InTag.ToString(), *GetNameSafe(this)), EDC_Input, EDV_Error);

	return nullptr;
}

bool UInputModule::IsInputActionActive(const FGameplayTag& InTag, int32 InPlayerIndex) const
{
	const UInputActionBase* Action = GetInputActionByTag(InTag, false);
	const APlayerController* PlayerController = PlayerRuntimes.IsValidIndex(InPlayerIndex) ? PlayerRuntimes[InPlayerIndex].PlayerController.Get() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = PlayerController
		? Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput)
		: nullptr;
	const FInputActionInstance* ActionInstance = PlayerInput && Action
		? PlayerInput->FindActionInstanceData(Action)
		: nullptr;
	return ActionInstance && ActionInstance->GetValue().IsNonZero();
}

bool UInputModule::ActivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(PlayerRuntimes[InPlayerIndex].LocalPlayer);
	if(!MappingContext || !MappingContext->Get() || !Player)
	{
		return false;
	}

	const FInputContextConfig* Config = ContextConfigs.FindByPredicate([InContextTag](const FInputContextConfig& Item)
	{
		return Item.ContextTag == InContextTag;
	});
	if(!Config)
	{
		return false;
	}

	return Player->SetWorldInputContext(GetWorld(), InContextTag, Config->MappingContext, Config->Priority, true);
}

bool UInputModule::DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(PlayerRuntimes[InPlayerIndex].LocalPlayer);
	if(!MappingContext || !MappingContext->Get() || !Player)
	{
		return false;
	}

	return Player->SetWorldInputContext(GetWorld(), InContextTag, MappingContext->Get(), 0, false);
}

bool UInputModule::IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex) const
{
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	const UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(PlayerRuntimes[InPlayerIndex].LocalPlayer);
	return MappingContext && MappingContext->Get() && Player && Player->HasWorldInputContext(GetWorld(), InContextTag);
}

bool UInputModule::MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason)
{
	if (!IsBusinessAdmissionOpen())
	{
		if (OutFailureReason) OutFailureReason->Reset();
		return false;
	}
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}

	TArray<FInputMappableEntry> Entries;
	PlayerMappableByActionTag.MultiFind(InActionTag, Entries);
	const FInputMappableEntry* Entry = Entries.FindByPredicate([InSlot](const FInputMappableEntry& Item)
	{
		return Item.Slot == InSlot;
	});
	if(!Entry)
	{
		return false;
	}
	return MapPlayerKeyByMappingName(Entry->MappingName, InNewKey, InSlot, InPlayerIndex, OutFailureReason);
}

bool UInputModule::MapPlayerKeyByMappingName(FName InMappingName, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason, const UObject* SettingOwner)
{
	if (!IsBusinessAdmissionOpen())
	{
		if (OutFailureReason) OutFailureReason->Reset();
		return false;
	}
	if(InMappingName.IsNone() || !PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}
	if (UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(PlayerRuntimes[InPlayerIndex].LocalPlayer))
		if (Player->HasSettingEdit() && !Player->IsSettingEditOwner(SettingOwner)) Player->NoteExternalInputSetting(InMappingName, static_cast<uint8>(InSlot));

	FMapPlayerKeyArgs Args;
	Args.MappingName = InMappingName;
	Args.Slot = InSlot;
	Args.NewKey = InNewKey;

	FGameplayTagContainer FailureReasons;
	PlayerRuntimes[InPlayerIndex].UserSettings->MapPlayerKey(Args, FailureReasons);
	if(OutFailureReason)
	{
		*OutFailureReason = FailureReasons;
	}
	return FailureReasons.IsEmpty();
}

bool UInputModule::ResetPlayerKeyByTag(FGameplayTag InActionTag, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}

	TArray<FInputMappableEntry> Entries;
	PlayerMappableByActionTag.MultiFind(InActionTag, Entries);
	bool bReset = false;
	TSet<FName> ResetMappingNames;
	for(const FInputMappableEntry& Entry : Entries)
	{
		if(ResetMappingNames.Contains(Entry.MappingName))
		{
			continue;
		}
		ResetMappingNames.Add(Entry.MappingName);

		bReset |= ResetPlayerKeyByMappingName(Entry.MappingName, InPlayerIndex);
	}
	return bReset;
}

bool UInputModule::ResetPlayerKeyByMappingName(FName InMappingName, int32 InPlayerIndex)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(InMappingName.IsNone() || !PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}
	if (UWHLocalPlayer* Player = Cast<UWHLocalPlayer>(PlayerRuntimes[InPlayerIndex].LocalPlayer))
		if (Player->HasSettingEdit()) Player->NoteExternalInputSettingRow(InMappingName);

	FMapPlayerKeyArgs Args;
	Args.MappingName = InMappingName;
	FGameplayTagContainer FailureReasons;
	PlayerRuntimes[InPlayerIndex].UserSettings->ResetAllPlayerKeysInRow(Args, FailureReasons);
	return FailureReasons.IsEmpty();
}

TArray<FPlayerKeyMapping> UInputModule::GetPlayerKeyMappingsByTag(FGameplayTag InActionTag, int32 InPlayerIndex) const
{
	TArray<FPlayerKeyMapping> Mappings;
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return Mappings;
	}

	TArray<FInputMappableEntry> Entries;
	PlayerMappableByActionTag.MultiFind(InActionTag, Entries);
	TSet<FName> MappingNames;
	for(const FInputMappableEntry& Entry : Entries)
	{
		MappingNames.Add(Entry.MappingName);
	}

	for(const FName MappingName : MappingNames)
	{
		for(const FPlayerKeyMapping& Mapping : GetPlayerKeyMappingsByMappingName(MappingName, InPlayerIndex))
		{
			const UInputActionBase* Action = Cast<UInputActionBase>(Mapping.GetAssociatedInputAction());
			if(Action && Action->ActionTag == InActionTag)
			{
				Mappings.AddUnique(Mapping);
			}
		}
	}
	Mappings.Sort([](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B)
	{
		return A.GetSlot() < B.GetSlot();
	});
	return Mappings;
}

TArray<FKey> UInputModule::GetKeysByActionTag(FGameplayTag InActionTag, int32 InPlayerIndex) const
{
	TArray<FKey> Keys;
	const TArray<FPlayerKeyMapping> PlayerMappings = GetPlayerKeyMappingsByTag(InActionTag, InPlayerIndex);
	for(const FPlayerKeyMapping& Mapping : PlayerMappings)
	{
		Keys.AddUnique(Mapping.GetCurrentKey());
	}

	for(const FInputContextConfig& Config : ContextConfigs)
	{
		if(!Config.MappingContext)
		{
			continue;
		}
		for(const FEnhancedActionKeyMapping& Mapping : Config.MappingContext->GetMappings())
		{
			const UInputActionBase* Action = Cast<UInputActionBase>(Mapping.Action);
			if(Action
				&& Action->ActionTag == InActionTag
				&& (!Mapping.IsPlayerMappable() || PlayerMappings.IsEmpty()))
			{
				Keys.AddUnique(Mapping.Key);
			}
		}
	}
	return Keys;
}

TArray<FPlayerKeyMapping> UInputModule::GetPlayerKeyMappingsByMappingName(FName InMappingName, int32 InPlayerIndex) const
{
	TArray<FPlayerKeyMapping> Mappings;
	if(InMappingName.IsNone() || !PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return Mappings;
	}

	for(const auto& Profile : PlayerRuntimes[InPlayerIndex].UserSettings->GetAllAvailableKeyProfiles())
	{
		if(const FKeyMappingRow* Row = Profile.Value->GetPlayerMappingRows().Find(InMappingName))
		{
			for(const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				Mappings.Add(Mapping);
			}
		}
	}
	Mappings.Sort([](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B)
	{
		return A.GetSlot() < B.GetSlot();
	});
	return Mappings;
}

TArray<FGameplayTag> UInputModule::GetAllMappableActions() const
{
	TArray<FGameplayTag> ActionTags;
	PlayerMappableByActionTag.GetKeys(ActionTags);
	ActionTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});
	ActionTags.SetNum(Algo::Unique(ActionTags));
	return ActionTags;
}

TArray<FInputMappableEntry> UInputModule::GetAllMappableEntries() const
{
	TArray<FInputMappableEntry> Entries;
	for(const TPair<FGameplayTag, FInputMappableEntry>& Pair : PlayerMappableByActionTag)
	{
		Entries.Add(Pair.Value);
	}
	Entries.Sort([](const FInputMappableEntry& A, const FInputMappableEntry& B)
	{
		if(A.ActionTag != B.ActionTag)
		{
			return A.ActionTag.ToString() < B.ActionTag.ToString();
		}
		if(A.MappingName != B.MappingName)
		{
			return A.MappingName.LexicalLess(B.MappingName);
		}
		return A.Slot < B.Slot;
	});
	return Entries;
}

TArray<FInputMappableEntry> UInputModule::GetMappableEntriesByActionTag(FGameplayTag InActionTag) const
{
	TArray<FInputMappableEntry> Entries;
	PlayerMappableByActionTag.MultiFind(InActionTag, Entries);
	Entries.Sort([](const FInputMappableEntry& A, const FInputMappableEntry& B)
	{
		if(A.MappingName != B.MappingName)
		{
			return A.MappingName.LexicalLess(B.MappingName);
		}
		return A.Slot < B.Slot;
	});
	return Entries;
}

ECommonInputType UInputModule::GetCurrentInputType(int32 InPlayerIndex) const
{
	return PlayerRuntimes.IsValidIndex(InPlayerIndex) && PlayerRuntimes[InPlayerIndex].CommonInputSubsystem
		? PlayerRuntimes[InPlayerIndex].CommonInputSubsystem->GetCurrentInputType()
		: ECommonInputType::Count;
}

bool UInputModule::IsUsingGamepad(int32 InPlayerIndex) const
{
	return GetCurrentInputType(InPlayerIndex) == ECommonInputType::Gamepad;
}

bool UInputModule::IsUsingMouseAndKeyboard(int32 InPlayerIndex) const
{
	return GetCurrentInputType(InPlayerIndex) == ECommonInputType::MouseAndKeyboard;
}

bool UInputModule::IsUsingTouch(int32 InPlayerIndex) const
{
	return GetCurrentInputType(InPlayerIndex) == ECommonInputType::Touch;
}
