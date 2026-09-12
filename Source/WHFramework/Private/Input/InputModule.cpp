// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Main/Base/ModuleBase.h"
#include "Event/Events/Input/Event_InputModeChanged.h"
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
	
	NativeInputMode = EInputMode::GameOnly;
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

	FInputManager::Get().AddInputManager(this);
	
	FInputManager::Get().SetNativeInputMode(NativeInputMode);

	for(UInputBindingBase* InputBinding : InputBindings)
	{
		InputBinding->OnInitialize(0);
	}
}

void UInputModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BuildPlayerRuntimes();

		UInputComponentBase* Component = UInputModuleStatics::GetInputComponent<UInputComponentBase>();

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

		for(UInputBindingBase* InputBinding : InputBindings)
		{
			InputBinding->OnBindInput(Component);
		}

		UInputModuleStatics::UpdateGlobalInputMode();
	}
}

void UInputModule::OnReset()
{
	Super::OnReset();

	for(UInputBindingBase* InputBinding : InputBindings)
	{
		InputBinding->OnReset();
	}
}

void UInputModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

	for(UInputBindingBase* InputBinding : InputBindings)
	{
		InputBinding->OnRefresh(DeltaSeconds);
	}
}

void UInputModule::OnPause()
{
	Super::OnPause();

	UCommonModuleStatics::GetPlayerController()->DisableInput(nullptr);
}

void UInputModule::OnUnPause()
{
	Super::OnUnPause();

	UCommonModuleStatics::GetPlayerController()->EnableInput(nullptr);
}

void UInputModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		FInputManager::Get().RemoveInputManager(this);

		for(UInputBindingBase* InputBinding : InputBindings)
		{
			InputBinding->OnTermination();
		}

		for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
		{
			if(Runtime.CommonInputSubsystem)
			{
				Runtime.CommonInputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
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

	auto AddMappings = [this](UInputMappingContext* MappingContext)
	{
		if(!MappingContext)
		{
			return;
		}

		for(const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			const UInputActionBase* Action = Cast<UInputActionBase>(Mapping.Action);
			if(!Action || !Action->ActionTag.IsValid())
			{
				continue;
			}

			const TObjectPtr<const UInputActionBase>* Existing = InputActionMap.Find(Action->ActionTag);
			ensureMsgf(!Existing || Existing->Get() == Action, TEXT("Input ActionTag collision: %s"), *Action->ActionTag.ToString());
			InputActionMap.Add(Action->ActionTag, Action);

			const FName MappingName = Mapping.GetMappingName();
			if(!MappingName.IsNone())
			{
				TArray<FInputMappableEntry> Entries;
				PlayerMappableByActionTag.MultiFind(Action->ActionTag, Entries);
				if(!Entries.ContainsByPredicate([MappingName](const FInputMappableEntry& Entry)
				{
					return Entry.MappingName == MappingName;
				}))
				{
					FInputMappableEntry Entry;
					Entry.ActionTag = Action->ActionTag;
					Entry.MappingName = MappingName;
					PlayerMappableByActionTag.Add(Action->ActionTag, Entry);
				}
			}
		}
	};

	for(const FInputContextConfig& Config : ContextConfigs)
	{
		if(!Config.ContextTag.IsValid() || !Config.MappingContext)
		{
			continue;
		}

		ensureMsgf(!InputContextMap.Contains(Config.ContextTag), TEXT("Input ContextTag collision: %s"), *Config.ContextTag.ToString());
		InputContextMap.Add(Config.ContextTag, Config.MappingContext);
		AddMappings(Config.MappingContext);
	}

}

void UInputModule::BuildPlayerRuntimes()
{
	for(FInputPlayerRuntime& Runtime : PlayerRuntimes)
	{
		if(Runtime.CommonInputSubsystem)
		{
			Runtime.CommonInputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
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

		if(Runtime.EnhancedInputSubsystem)
		{
			Runtime.UserSettings = Runtime.EnhancedInputSubsystem->GetUserSettings();
		}

		if(APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld()))
		{
			Runtime.InputComponent = Cast<UInputComponentBase>(PlayerController->InputComponent);
		}

		if(Runtime.CommonInputSubsystem)
		{
			Runtime.CommonInputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged, PlayerIndex);
		}
	}
}

void UInputModule::HandleInputMethodChanged(ECommonInputType InInputType, int32 InPlayerIndex)
{
	OnInputTypeChanged.Broadcast(InPlayerIndex, InInputType);
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

void UInputModule::SetNativeInputMode(EInputMode InInputMode)
{
	NativeInputMode = InInputMode;
	FInputManager::Get().SetNativeInputMode(InInputMode);
}

UInputBindingBase* UInputModule::GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex) const
{
	const FName InputBindingName = InClass->GetDefaultObject<UInputBindingBase>()->GetInputBindingName();
	return GetInputBindingByName(InputBindingName, InPlayerIndex, InClass);
}

UInputBindingBase* UInputModule::GetInputBindingByName(const FName InName, int32 InPlayerIndex, TSubclassOf<UInputBindingBase> InClass) const
{
	for(UInputBindingBase* InputBinding : InputBindings)
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
			MappingNames.Add(Mapping.GetMappingName());
		}
	}
	return MappingNames;
}

TArray<FPlayerKeyMapping> UInputModule::GetAllPlayerKeyMappings(int32 InPlayerIndex)
{
	TArray<FPlayerKeyMapping> Mappings;
	if(const UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>(InPlayerIndex))
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

	if(Config->bRegisterWithSettings && PlayerRuntimes[InPlayerIndex].UserSettings)
	{
		PlayerRuntimes[InPlayerIndex].UserSettings->RegisterInputMappingContext(Config->MappingContext);
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
	if(Entries.IsEmpty())
	{
		return false;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = Entries[0].MappingName;
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
	for(const FInputMappableEntry& Entry : Entries)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Entry.MappingName;
		FGameplayTagContainer FailureReasons;
		PlayerRuntimes[InPlayerIndex].UserSettings->ResetAllPlayerKeysInRow(Args, FailureReasons);
		bReset |= FailureReasons.IsEmpty();
	}
	return bReset;
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
	for(const FInputMappableEntry& Entry : Entries)
	{
		for(const auto& Profile : PlayerRuntimes[InPlayerIndex].UserSettings->GetAllAvailableKeyProfiles())
		{
			if(const FKeyMappingRow* Row = Profile.Value->GetPlayerMappingRows().Find(Entry.MappingName))
			{
				for(const FPlayerKeyMapping& Mapping : Row->Mappings)
				{
					Mappings.Add(Mapping);
				}
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
	return ActionTags;
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
