// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

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
#include "Common/CommonModuleStatics.h"
#include "Main/MainModuleStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Input/InputManager.h"
#include "Input/Base/InputActionBase.h"
#include "SaveGame/Module/InputSaveGame.h"
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

	ModuleSaveGame = UInputSaveGame::StaticClass();
	
	DefaultInputMode = EInputMode::GameOnly;
}

UInputModule::~UInputModule()
{
	TERMINATION_MODULE(UInputModule)
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

	TERMINATION_MODULE(UInputModule)
}
#endif

void UInputModule::OnInitialize()
{
	Super::OnInitialize();

	BuildInputCaches();

	FInputManager::Get().OnInputModeChanged.AddUObject(this, &ThisClass::HandleGlobalInputModeChanged);
	FInputManager::Get().SetDefaultInputMode(DefaultInputMode);
}

void UInputModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BuildPlayerRuntimes();

		for(const FInputContextConfig& Config : ContextConfigs)
		{
			if(Config.bAutoActivate)
			{
				for(int32 PlayerIndex = 0; PlayerIndex < PlayerRuntimes.Num(); ++PlayerIndex)
				{
					ActivateInputContext(Config.ContextTag, PlayerIndex);
				}
			}
		}

		UInputModuleStatics::UpdateGlobalInputMode();
	}
}

void UInputModule::OnReset()
{
	Super::OnReset();

	for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		for(UInputBindingBase* InputBinding : Runtime.InputBindings)
		{
			if(InputBinding)
			{
				InputBinding->OnReset();
			}
		}
	}
}

void UInputModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

	for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		for(UInputBindingBase* InputBinding : Runtime.InputBindings)
		{
			if(InputBinding)
			{
				InputBinding->OnRefresh(DeltaSeconds);
			}
		}
	}
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
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		FInputManager::Get().OnInputModeChanged.RemoveAll(this);
		FInputManager::Get().SetCommonUIInputMode(TOptional<EInputMode>());

		for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
		{
			for(UInputBindingBase* InputBinding : Runtime.InputBindings)
			{
				if(InputBinding)
				{
					InputBinding->OnTermination();
				}
			}
			Runtime.InputBindings.Reset();

			if(Runtime.CommonInputSubsystem)
			{
				Runtime.CommonInputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
			}
			if(Runtime.CommonUIActionRouter)
			{
				Runtime.CommonUIActionRouter->OnActiveInputConfigChanged().RemoveAll(this);
			}
		}
		PlayerRuntimes.Reset();
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
	for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		for(UInputBindingBase* InputBinding : Runtime.InputBindings)
		{
			if(InputBinding)
			{
				InputBinding->OnTermination();
			}
		}
		if(Runtime.CommonInputSubsystem)
		{
			Runtime.CommonInputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
		}
		if(Runtime.CommonUIActionRouter)
		{
			Runtime.CommonUIActionRouter->OnActiveInputConfigChanged().RemoveAll(this);
		}
	}
	PlayerRuntimes.Reset();

	if(!GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}

	const TArray<ULocalPlayer*>& LocalPlayers = GetWorld()->GetGameInstance()->GetLocalPlayers();
	for(int32 PlayerIndex = 0; PlayerIndex < LocalPlayers.Num(); ++PlayerIndex)
	{
		ULocalPlayer* LocalPlayer = LocalPlayers[PlayerIndex];
		FInputPlayerRuntime& Runtime = PlayerRuntimes.AddDefaulted_GetRef();
		Runtime.LocalPlayer = LocalPlayer;
		Runtime.EnhancedInputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		Runtime.CommonInputSubsystem = LocalPlayer->GetSubsystem<UCommonInputSubsystem>();
		Runtime.CommonUIActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>();

		if(Runtime.EnhancedInputSubsystem)
		{
			Runtime.UserSettings = Runtime.EnhancedInputSubsystem->GetUserSettings();
			if(Runtime.UserSettings)
			{
				for(const FInputContextConfig& Config : ContextConfigs)
				{
					if(Config.bRegisterWithSettings && Config.MappingContext)
					{
						Runtime.UserSettings->RegisterInputMappingContext(Config.MappingContext);
					}
				}
			}
		}

		if(APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld()))
		{
			Runtime.InputComponent = Cast<UInputComponentBase>(PlayerController->InputComponent);
		}

		if(Runtime.CommonInputSubsystem)
		{
			Runtime.CommonInputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged, PlayerIndex);
		}
		if(Runtime.CommonUIActionRouter)
		{
			Runtime.CommonUIActionRouter->OnActiveInputConfigChanged().AddUObject(
				this,
				&ThisClass::HandleCommonUIInputConfigChanged,
				PlayerIndex);
		}

		for(UInputBindingBase* InputBindingTemplate : InputBindings)
		{
			if(!InputBindingTemplate)
			{
				continue;
			}

			UInputBindingBase* InputBinding = DuplicateObject<UInputBindingBase>(
				InputBindingTemplate,
				this);
			Runtime.InputBindings.Add(InputBinding);
			InputBinding->OnInitialize(PlayerIndex);
			if(Runtime.InputComponent)
			{
				InputBinding->OnBindInput(Runtime.InputComponent);
			}
		}
	}

	RefreshCommonUIInputMode(0);
}

void UInputModule::HandleInputMethodChanged(ECommonInputType InInputType, int32 InPlayerIndex)
{
	OnInputTypeChanged.Broadcast(InPlayerIndex, InInputType);
	UEventModuleStatics::BroadcastEvent<FEventInputTypeChanged>(this, { InPlayerIndex, InInputType });
}

void UInputModule::HandleCommonUIInputConfigChanged(FUIInputConfig InInputConfig, int32 InPlayerIndex)
{
	if(InPlayerIndex != 0 || !PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return;
	}

	const UCommonUIActionRouterBase* ActionRouter = PlayerRuntimes[InPlayerIndex].CommonUIActionRouter;
	ApplyCommonUIInputConfig(ActionRouter && ActionRouter->GetLeafmostActivatableWidget() ? &InInputConfig : nullptr);
}

void UInputModule::HandleGlobalInputModeChanged(EInputMode InPreviousInputMode, EInputMode InInputMode)
{
	UEventModuleStatics::BroadcastEvent<FEventInputModeChanged>(this, { InInputMode, InPreviousInputMode });
}

void UInputModule::ApplyCommonUIInputConfig(const FUIInputConfig* InInputConfig)
{
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

	FInputManager::Get().SetCommonUIInputMode(InputMode);
}

void UInputModule::RefreshCommonUIInputMode(int32 InPlayerIndex)
{
	if(InPlayerIndex != 0 || !PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return;
	}

	const UCommonUIActionRouterBase* ActionRouter = PlayerRuntimes[InPlayerIndex].CommonUIActionRouter;
	const UCommonActivatableWidget* ActiveWidget = ActionRouter
		? ActionRouter->GetLeafmostActivatableWidget()
		: nullptr;
	const TOptional<FUIInputConfig> InputConfig = ActiveWidget
		? ActiveWidget->GetDesiredInputConfig()
		: TOptional<FUIInputConfig>();
	ApplyCommonUIInputConfig(InputConfig.IsSet() ? &InputConfig.GetValue() : nullptr);
}

void UInputModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInputModuleSaveData>();
	if(SaveData.IsSaved())
	{
		UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>();

		UCommonModuleStatics::LoadObjectDataFromMemory(Settings, SaveData.GetDatas());

		RefreshData();
	}
}

void UInputModule::UnloadData(EPhase InPhase)
{
	
}

void UInputModule::RefreshData()
{
	LocalSaveData = GetSaveDataRef<FInputModuleSaveData>(true);
}

FSaveData* UInputModule::ToData()
{
	FInputModuleSaveData& SaveData = GetMutableSaveData<FInputModuleSaveData>();
	SaveData = FInputModuleSaveData();

	UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>();

	UCommonModuleStatics::SaveObjectDataToMemory(Settings, SaveData.GetDatas());

	return &SaveData;
}

FSaveData* UInputModule::GetData()
{
	if(!LocalSaveData.IsValid())
	{
		LocalSaveData = ToData()->CastRef<FInputModuleSaveData>();
	}
	return &LocalSaveData;
}

FString UInputModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("GlobalInputMode: %s"), *UCommonModuleStatics::GetEnumAuthoredNameByValue(TEXT("/Script/WHFrameworkCore.EInputMode"), (int32)UInputModuleStatics::GetGlobalInputMode()));
}

void UInputModule::SetDefaultInputMode(EInputMode InInputMode)
{
	DefaultInputMode = InInputMode;
	FInputManager::Get().SetDefaultInputMode(InInputMode);
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
	const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController(InPlayerIndex);
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
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PlayerRuntimes[InPlayerIndex].EnhancedInputSubsystem;
	if(!MappingContext || !MappingContext->Get() || !Subsystem)
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

	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	Subsystem->AddMappingContext(Config->MappingContext, Config->Priority, Options);
	return true;
}

bool UInputModule::DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PlayerRuntimes[InPlayerIndex].EnhancedInputSubsystem;
	if(!MappingContext || !MappingContext->Get() || !Subsystem)
	{
		return false;
	}

	Subsystem->RemoveMappingContext(MappingContext->Get());
	return true;
}

bool UInputModule::IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex) const
{
	if(!PlayerRuntimes.IsValidIndex(InPlayerIndex))
	{
		return false;
	}

	const TObjectPtr<UInputMappingContext>* MappingContext = InputContextMap.Find(InContextTag);
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = PlayerRuntimes[InPlayerIndex].EnhancedInputSubsystem;
	return MappingContext && MappingContext->Get() && Subsystem && Subsystem->HasMappingContext(MappingContext->Get());
}

bool UInputModule::MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason)
{
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

bool UInputModule::MapPlayerKeyByMappingName(FName InMappingName, FKey InNewKey, EPlayerMappableKeySlot InSlot, int32 InPlayerIndex, FGameplayTagContainer* OutFailureReason)
{
	if(InMappingName.IsNone() || !PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}

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
	if(InMappingName.IsNone() || !PlayerRuntimes.IsValidIndex(InPlayerIndex) || !PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		return false;
	}

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
		: ECommonInputType::MouseAndKeyboard;
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
