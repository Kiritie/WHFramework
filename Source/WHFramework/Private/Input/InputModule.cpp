// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleBPLibrary.h"
#include "Main/Base/ModuleBase.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Input/EventHandle_ChangeInputMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Input/InputManager.h"
#include "Common/CommonBPLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/Base/InputActionBase.h"
#include "Input/Components/InputComponentBase.h"
#include "SaveGame/Module/InputSaveGame.h"
#include "CommonInputSubsystem.h"
#include "PlayerMappableInputConfig.h"
#include "Debug/DebugModuleTypes.h"

IMPLEMENTATION_MODULE(AInputModule)

// Sets default values
AInputModule::AInputModule()
{
	ModuleName = FName("InputModule");

	ModuleSaveGame = UInputSaveGame::StaticClass();
	
	AddKeyShortcut(FName("CameraPanMove"), FKey("MiddleMouseButton"));
	AddKeyShortcut(FName("CameraRotate"));
	AddKeyShortcut(FName("CameraZoom"));

	// static ConstructorHelpers::FObjectFinder<UInputMappingContext> CameraMovementMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/IMC_CameraMovement.IMC_CameraMovement'"));
	// if(CameraMovementMapping.Succeeded())
	// {
	// 	ActionContexts.Add(CameraMovementMapping.Object);
	// }
	//
	// static ConstructorHelpers::FObjectFinder<UInputMappingContext> PlayerMovementMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/IMC_PlayerMovement.IMC_PlayerMovement'"));
	// if(PlayerMovementMapping.Succeeded())
	// {
	// 	ActionContexts.Add(PlayerMovementMapping.Object);
	// }

	AddTouchMapping(FInputTouchMapping(IE_Pressed, FInputTouchHandlerSignature::CreateUObject(this, &AInputModule::TouchPressed)));
	AddTouchMapping(FInputTouchMapping(IE_Released, FInputTouchHandlerSignature::CreateUObject(this, &AInputModule::TouchReleased)));
	AddTouchMapping(FInputTouchMapping(IE_Repeat, FInputTouchHandlerSignature::CreateUObject(this, &AInputModule::TouchMoved)));

	TouchInputRate = 1.f;
	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

AInputModule::~AInputModule()
{
	TERMINATION_MODULE(AInputModule)
}

#if WITH_EDITOR
void AInputModule::OnGenerate()
{
	Super::OnGenerate();
}

void AInputModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AInputModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AInputModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		ApplyKeyMappings();
		ApplyTouchMappings();
		
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetPlayerController()->GetLocalPlayer()))
		{
			for (const FInputConfigMapping& Pair : ConfigMapping)
			{
				FInputConfigMapping::RegisterPair(Pair);
				if (Pair.bShouldActivateAutomatically && Pair.CanBeActivated())
				{
					if (UPlayerMappableInputConfig* InputConfig = Pair.Config.LoadSynchronous())
					{
						FModifyContextOptions Options = {};
						Options.bIgnoreAllPressedKeysUntilRelease = false;
						Subsystem->AddPlayerMappableConfig(InputConfig, Options);

						if(UInputComponentBase* LyraIC = Cast<UInputComponentBase>(GetPlayerController()->InputComponent))
						{
							LyraIC->AddInputMappings(InputConfig, Subsystem);

							OnBindAction(LyraIC, InputConfig);
						}
					}
				}
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bAutoSaveModule)
		{
			Load();
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		UpdateInputMode();
	}
}

void AInputModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AInputModule::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	TouchPressedCount = 0;
}

void AInputModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AInputModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AInputModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bAutoSaveModule)
		{
			Save();
		}
	}
}

void AInputModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInputModuleSaveData>();

	for(auto& Iter : SaveData.KeyShortcuts)
	{
		if(KeyShortcuts.Contains(Iter.Key))
		{
			KeyShortcuts[Iter.Key].Key = Iter.Value;
		}
	}

	for(int32 i = 0; i < SaveData.ActionMappings.Num(); i++)
	{
		// if(ActionContexts.IsValidIndex(i))
		{
			// const_cast<TArray<FEnhancedActionKeyMapping>&>(ActionContexts[i]->GetMappings()) = SaveData.ActionMappings[i].Mappings;
		}
	}

	for(auto& Iter : SaveData.KeyMappings)
	{
		if(KeyMappings.Contains(Iter.Key))
		{
			KeyMappings[Iter.Key].Key = Iter.Value;
		}
	}
}

void AInputModule::UnloadData(EPhase InPhase)
{
}

FSaveData* AInputModule::ToData()
{
	static FInputModuleSaveData SaveData;
	SaveData = FInputModuleSaveData();

	for(auto& Iter : KeyShortcuts)
	{
		if(SaveData.KeyShortcuts.Contains(Iter.Key))
		{
			SaveData.KeyShortcuts[Iter.Key] = Iter.Value.Key;
		}
	}

	// for(int32 i = 0; i < ActionContexts.Num(); i++)
	{
		// SaveData.ActionMappings.EmplaceAt_GetRef(i).Mappings = ActionContexts[i]->GetMappings();
	}

	for(auto& Iter : KeyMappings)
	{
		if(SaveData.KeyMappings.Contains(Iter.Key))
		{
			SaveData.KeyMappings[Iter.Key] = Iter.Value.Key;
		}
	}

	return &SaveData;
}

FInputKeyShortcut AInputModule::GetKeyShortcutByName(const FName InName) const
{
	if(KeyShortcuts.Contains(InName))
	{
		return KeyShortcuts[InName];
	}
	return FInputKeyShortcut();
}

void AInputModule::AddKeyShortcut(const FName InName, const FInputKeyShortcut& InKeyShortcut)
{
	if(!KeyShortcuts.Contains(InName))
	{
		KeyShortcuts.Add(InName, InKeyShortcut);
	}
}

void AInputModule::RemoveKeyShortcut(const FName InName)
{
	if(KeyShortcuts.Contains(InName))
	{
		KeyShortcuts.Remove(InName);
	}
}

void AInputModule::OnBindAction_Implementation(UInputComponentBase* InInputComponent, UPlayerMappableInputConfig* InInputConfig)
{
	InInputComponent->BindNativeAction(InInputConfig, GameplayTags::InputTag_TurnCamera, ETriggerEvent::Triggered, this, &ThisClass::TurnCamera);
	// if(InInputConfig->ActionTag == TEXT("TurnCamera"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::TurnCamera);
	// }
	// else if(InInputConfig->ActionTag == TEXT("LookUpCamera"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::LookUpCamera);
	// }
	// else if(InInputConfig->ActionTag == TEXT("PanHCamera"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::PanHCamera);
	// }
	// else if(InInputConfig->ActionTag == TEXT("PanVCamera"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::PanVCamera);
	// }
	// else if(InInputConfig->ActionTag == TEXT("ZoomCamera"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::ZoomCamera);
	// }
	// else if(InInputConfig->ActionTag == TEXT("TurnPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::TurnPlayer);
	// }
	// else if(InInputConfig->ActionTag == TEXT("MoveHPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::MoveHPlayer);
	// }
	// else if(InInputConfig->ActionTag == TEXT("MoveVPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::MoveVPlayer);
	// }
	// else if(InInputConfig->ActionTag == TEXT("MoveForwardPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::MoveForwardPlayer);
	// }
	// else if(InInputConfig->ActionTag == TEXT("MoveRightPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::MoveRightPlayer);
	// }
	// else if(InInputConfig->ActionTag == TEXT("MoveUpPlayer"))
	// {
	// 	InInputComponent->BindAction(InInputConfig, ETriggerEvent::Triggered, this, &AInputModule::MoveUpPlayer);
	// }
}

void AInputModule::AddKeyMapping(const FName InName, const FInputKeyMapping& InKeyMapping)
{
	if(!KeyMappings.Contains(InName))
	{
		KeyMappings.Add(InName, InKeyMapping);
	}
}

void AInputModule::RemoveKeyMapping(const FName InName)
{
	if(KeyMappings.Contains(InName))
	{
		KeyMappings.Remove(InName);
	}
}

void AInputModule::ApplyKeyMappings()
{
	GetPlayerController()->InputComponent->KeyBindings.Empty();
	for(auto& Iter : KeyMappings)
	{
		FInputKeyBinding KB(FInputChord(Iter.Key, false, false, false, false), Iter.Value.Event);
		KB.KeyDelegate.BindDelegate(Iter.Value.Delegate.IsBound() ? Iter.Value.Delegate.GetUObject() : Iter.Value.DynamicDelegate.GetUObject(), Iter.Value.Delegate.IsBound() ? Iter.Value.Delegate.TryGetBoundFunctionName() : Iter.Value.DynamicDelegate.GetFunctionName());
		GetPlayerController()->InputComponent->KeyBindings.Emplace(MoveTemp(KB));
	}
}

void AInputModule::ApplyTouchMappings()
{
	GetPlayerController()->InputComponent->TouchBindings.Empty();
	for(auto& Iter : TouchMappings)
	{
		FInputTouchBinding TB(Iter.Event);
		TB.TouchDelegate.BindDelegate(Iter.Delegate.IsBound() ? Iter.Delegate.GetUObject() : Iter.DynamicDelegate.GetUObject(), Iter.Delegate.IsBound() ? Iter.Delegate.TryGetBoundFunctionName() : Iter.DynamicDelegate.GetFunctionName());
		GetPlayerController()->InputComponent->TouchBindings.Emplace(MoveTemp(TB));
	}
}

void AInputModule::AddTouchMapping(const FInputTouchMapping& InKeyMapping)
{
	TouchMappings.Add(InKeyMapping);
}

void AInputModule::SetControllerPlatform(const FName InControllerPlatform)
{
	if (ControllerPlatform != InControllerPlatform)
	{
		ControllerPlatform = InControllerPlatform;

		// Apply the change to the common input subsystem so that we refresh any input icons we're using.
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetTypedOuter<ULocalPlayer>()))
		{
			InputSubsystem->SetGamepadInputType(ControllerPlatform);
		}
	}
}

FName AInputModule::GetControllerPlatform() const
{
	return ControllerPlatform;
}

const UInputActionBase* AInputModule::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FInputConfigMapping& Pair : ConfigMapping)
	{
		if (UPlayerMappableInputConfig* InputConfig = Pair.Config.LoadSynchronous())
		{
			for(auto Iter : InputConfig->GetMappingContexts())
			{
				for(auto Iter2 : Iter.Key->GetMappings())
				{
					if(auto InputAction = Cast<UInputActionBase>(Iter2.Action))
					{
						if(InputAction->ActionTag == InputTag)
						{
							return InputAction;
						}
					}
				}
			}
		}
	}
	
	if (bLogNotFound)
	{
		WHLog(FString::Printf(TEXT("Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this)), EDC_Input, EDV_Warning);
	}

	return nullptr;
}

void AInputModule::RegisterInputConfig(ECommonInputType Type, const UPlayerMappableInputConfig* NewConfig, const bool bIsActive)
{
	if (NewConfig)
	{
		const int32 ExistingConfigIdx = RegisteredInputConfigs.IndexOfByPredicate( [&NewConfig](const FLoadedInputConfigMapping& Pair) { return Pair.Config == NewConfig; } );
		if (ExistingConfigIdx == INDEX_NONE)
		{
			const int32 NumAdded = RegisteredInputConfigs.Add(FLoadedInputConfigMapping(NewConfig, Type, bIsActive));
			if (NumAdded != INDEX_NONE)
			{
				OnInputConfigRegistered.Broadcast(RegisteredInputConfigs[NumAdded]);
			}	
		}
	}
}

int32 AInputModule::UnregisterInputConfig(const UPlayerMappableInputConfig* ConfigToRemove)
{
	if (ConfigToRemove)
	{
		const int32 Index = RegisteredInputConfigs.IndexOfByPredicate( [&ConfigToRemove](const FLoadedInputConfigMapping& Pair) { return Pair.Config == ConfigToRemove; } );
		if (Index != INDEX_NONE)
		{
			RegisteredInputConfigs.RemoveAt(Index);
			return 1;
		}
			
	}
	return INDEX_NONE;
}

const UPlayerMappableInputConfig* AInputModule::GetInputConfigByName(FName ConfigName) const
{
	for (const FLoadedInputConfigMapping& Pair : RegisteredInputConfigs)
	{
		if (Pair.Config->GetConfigName() == ConfigName)
		{
			return Pair.Config;
		}
	}
	return nullptr;
}

void AInputModule::GetRegisteredInputConfigsOfType(ECommonInputType Type, TArray<FLoadedInputConfigMapping>& OutArray) const
{
	OutArray.Empty();

	// If "Count" is passed in then 
	if (Type == ECommonInputType::Count)
	{
		OutArray = RegisteredInputConfigs;
		return;
	}
	
	for (const FLoadedInputConfigMapping& Pair : RegisteredInputConfigs)
	{
		if (Pair.Type == Type)
		{
			OutArray.Emplace(Pair);
		}
	}
}

void AInputModule::GetAllMappingNamesFromKey(const FKey InKey, TArray<FName>& OutActionNames)
{
	if (InKey == EKeys::Invalid)
	{
		return;
	}

	// adding any names of actions that are bound to that key
	for (const FLoadedInputConfigMapping& Pair : RegisteredInputConfigs)
	{
		if (Pair.Type == ECommonInputType::MouseAndKeyboard)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Pair.Config->GetPlayerMappableKeys())
			{
				FName MappingName(Mapping.PlayerMappableOptions.DisplayName.ToString());
				FName ActionName = Mapping.PlayerMappableOptions.Name;
				// make sure it isn't custom bound as well
				if (const FKey* MappingKey = CustomKeyboardConfig.Find(ActionName))
				{
					if (*MappingKey == InKey)
					{
						OutActionNames.Add(MappingName);
					}
				}
				else
				{
					if (Mapping.Key == InKey)
					{
						OutActionNames.Add(MappingName);
					}
				}
			}
		}
	}
}

void AInputModule::GetAllMappingByName(const FName InName, TArray<FEnhancedActionKeyMapping>& OutMappings)
{
	// adding any names of actions that are bound to that key
	for (const FLoadedInputConfigMapping& Pair : RegisteredInputConfigs)
	{
		if (Pair.Type == ECommonInputType::MouseAndKeyboard)
		{
			for (auto Iter : Pair.Config->GetPlayerMappableKeys())
			{
				if(Iter.GetMappingName() == InName)
				{
					OutMappings.Add(Iter);
				}
			}
		}
	}
}

void AInputModule::GetAllMappingByDisplayName(const FText InDisplayName, TArray<FEnhancedActionKeyMapping>& OutMappings)
{
	// adding any names of actions that are bound to that key
	for (const FLoadedInputConfigMapping& Pair : RegisteredInputConfigs)
	{
		if (Pair.Type == ECommonInputType::MouseAndKeyboard)
		{
			FEnhancedActionKeyMapping OutMapping;
	
			for (auto Iter : Pair.Config->GetPlayerMappableKeys())
			{
				if(Iter.PlayerMappableOptions.DisplayName.EqualTo(InDisplayName))
				{
					OutMappings.Add(Iter);
				}
			}
		}
	}
}

void AInputModule::AddOrUpdateCustomKeyboardBindings(const FName MappingName, const FKey NewKey, ULocalPlayer* LocalPlayer)
{
	// Tell the enhanced input subsystem for this local player that we should remap some input! Woo
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->AddPlayerMappedKeyInSlot(MappingName, NewKey);
	}
}

FKey AInputModule::GetCustomKeyboardBindings(const FName MappingName, ULocalPlayer* LocalPlayer)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		return Subsystem->GetPlayerMappedKeyInSlot(MappingName);
	}
	return FKey();
}

void AInputModule::ResetKeybindingToDefault(const FName MappingName, ULocalPlayer* LocalPlayer)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->RemoveAllPlayerMappedKeysForMapping(MappingName);
	}
}

void AInputModule::ResetKeybindingsToDefault(ULocalPlayer* LocalPlayer)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->RemoveAllPlayerMappedKeys();
	}
}

void AInputModule::TurnCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || ACameraModule::Get()->IsControllingMove()) return;

	if(GetKeyShortcutByName(FName("CameraRotate")).IsPressing(GetPlayerController(), true))
	{
		ACameraModule::Get()->AddCameraRotationInput(InValue.Get<float>(), 0.f);
	}
}

void AInputModule::LookUpCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || ACameraModule::Get()->IsControllingMove()) return;

	if(GetKeyShortcutByName(FName("CameraRotate")).IsPressing(GetPlayerController(), true))
	{
		ACameraModule::Get()->AddCameraRotationInput(0.f, ACameraModule::Get()->IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>());
	}
}

void AInputModule::PanHCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcutByName(FName("CameraPanMove")).IsPressing(GetPlayerController()))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f);
		ACameraModule::Get()->AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void AInputModule::PanVCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcutByName(FName("CameraPanMove")).IsPressing(GetPlayerController()))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f);
		ACameraModule::Get()->AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void AInputModule::ZoomCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcutByName(FName("CameraZoom")).IsPressing(GetPlayerController(), true))
	{
		ACameraModule::Get()->AddCameraDistanceInput(-InValue.Get<float>());
	}
}

void AInputModule::TurnPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::MoveHPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveH(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::MoveVPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveV(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::MoveForwardPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::MoveRightPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::MoveUpPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void AInputModule::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			if(TouchReleaseTimerHandle1.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch2:
		{
			if(TouchReleaseTimerHandle2.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch3:
		{
			if(TouchReleaseTimerHandle3.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		default: break;
	}
}

void AInputModule::TouchPressedImpl()
{
	TouchPressedCount++;

	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

void AInputModule::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle1, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch2:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle2, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch3:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle3, TimerDelegate, 0.15f, false);
			break;
		}
		default: break;
	}
}

void AInputModule::TouchReleasedImpl(ETouchIndex::Type InTouchIndex)
{
	TouchPressedCount--;
	if(TouchPressedCount < 0)
	{
		TouchPressedCount = 0;
	}
	
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle1);
			break;
		}
		case ETouchIndex::Touch2:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle2);
			break;
		}
		case ETouchIndex::Touch3:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle3);
			break;
		}
		default: break;
	}
}

void AInputModule::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	if(TouchPressedCount <= 0) return;
	
	if(TouchPressedCount == 1)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetPlayerController()->GetInputTouchState(InTouchIndex, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			ACameraModule::Get()->AddCameraRotationInput((TouchLocationX - TouchLocationPrevious.X) * TouchInputRate, -(TouchLocationY - TouchLocationPrevious.Y) * TouchInputRate);
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
	else if(TouchPressedCount == 2)
	{
		float TouchLocationX1 = 0.f;
		float TouchLocationY1 = 0.f;
		bool bIsCurrentPressed1 = false;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch1, TouchLocationX1, TouchLocationY1, bIsCurrentPressed1);
		
		float TouchLocationX2;
		float TouchLocationY2;
		bool bIsCurrentPressed2;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch2, TouchLocationX2, TouchLocationY2, bIsCurrentPressed2);
		
		const float TouchCurrentPinchValue = FVector2D::Distance(FVector2D(TouchLocationX1, TouchLocationY1), FVector2D(TouchLocationX2, TouchLocationY2));
		if(TouchPinchValuePrevious != -1.f)
		{
			UCameraModuleBPLibrary::AddCameraDistanceInput(-(TouchCurrentPinchValue - TouchPinchValuePrevious) * TouchInputRate);
		}
		TouchPinchValuePrevious = TouchCurrentPinchValue;
	}
	else if(TouchPressedCount == 3)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			const FRotator Rotation = GetPlayerController()->GetControlRotation();
			const FVector DirectionH = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TouchLocationX - TouchLocationPrevious.X);
			const FVector DirectionV = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -(TouchLocationY - TouchLocationPrevious.Y);
			ACameraModule::Get()->AddCameraMovementInput(DirectionH + DirectionV, TouchInputRate * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f));
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
}

AWHPlayerController* AInputModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UCommonBPLibrary::GetPlayerController<AWHPlayerController>();
	}
	return PlayerController;
}

void AInputModule::UpdateInputMode()
{
	EInputMode InputMode = EInputMode::None;
	for (const auto Iter : AMainModule::GetAllModule())
	{
		if(const IInputManager* InputManager = Cast<IInputManager>(Iter))
		{
			if ((int32)InputManager->GetNativeInputMode() > (int32)InputMode)
			{
				InputMode = InputManager->GetNativeInputMode();
			}
		}
	}
	SetGlobalInputMode(InputMode);
}

void AInputModule::SetGlobalInputMode(EInputMode InInputMode)
{
	if(GlobalInputMode != InInputMode)
	{
		GlobalInputMode = InInputMode;
		switch (InInputMode)
		{
			case EInputMode::None:
			{
				GetPlayerController()->SetInputMode(FInputModeNone());
				GetPlayerController()->bShowMouseCursor = false;
				break;
			}
			case EInputMode::GameOnly:
			{
				GetPlayerController()->SetInputMode(FInputModeGameOnly());
				GetPlayerController()->bShowMouseCursor = false;
				break;
			}
			case EInputMode::UIOnly:
			{
				GetPlayerController()->SetInputMode(FInputModeUIOnly());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI:
			{
				GetPlayerController()->SetInputMode(FInputModeGameAndUI());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI_NotHideCursor:
			{
				GetPlayerController()->SetInputMode(FInputModeGameAndUI_NotHideCursor());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			default: break;
		}
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ChangeInputMode::StaticClass(), EEventNetType::Multicast, this, { &GlobalInputMode });
	}
}
