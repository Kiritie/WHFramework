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
	
	AddKeyShortcut(FName("CameraPanMove"));
	AddKeyShortcut(FName("CameraRotate"));
	AddKeyShortcut(FName("CameraZoom"));

	static ConstructorHelpers::FObjectFinder<UPlayerMappableInputConfig> CameraMovementMapping(TEXT("/Script/EnhancedInput.PlayerMappableInputConfig'/WHFramework/Input/DataAssets/PMIC_MouseAndKeyboard.PMIC_MouseAndKeyboard'"));
	if(CameraMovementMapping.Succeeded())
	{
		ConfigMapping.Add(FInputConfigMapping(CameraMovementMapping.Object, ECommonInputType::MouseAndKeyboard));
	}

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
			for (const auto& Iter : ConfigMapping)
			{
				if (UPlayerMappableInputConfig* InputConfig = Iter.Config.LoadSynchronous())
				{
					RegisterInputConfig(Iter.Type, InputConfig, false);
					if (Iter.bShouldActivateAutomatically && Iter.CanBeActivated())
					{
						FModifyContextOptions Options = {};
						Options.bIgnoreAllPressedKeysUntilRelease = false;
						Subsystem->AddPlayerMappableConfig(InputConfig, Options);
						if(UInputComponentBase* InputComp = Cast<UInputComponentBase>(GetPlayerController()->InputComponent))
						{
							OnBindAction(InputComp, InputConfig);
						}
					}
				}
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
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
		if(bModuleAutoSave)
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

	for(auto& Iter : SaveData.ActionMappings)
	{
		AddOrUpdateCustomKeyboardBindings(Iter.PlayerMappableOptions.Name, Iter.Key);
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
	CustomKeyMappings.Empty();
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
	
	SaveData.ActionMappings = GetAllPlayerMappableActionKeyMappings();

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
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_TurnCamera, ETriggerEvent::Triggered, this, &AInputModule::TurnCamera);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_LookUpCamera, ETriggerEvent::Triggered, this, &AInputModule::LookUpCamera);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_PanHCamera, ETriggerEvent::Triggered, this, &AInputModule::PanHCamera);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_PanVCamera, ETriggerEvent::Triggered, this, &AInputModule::PanVCamera);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_ZoomCamera, ETriggerEvent::Triggered, this, &AInputModule::ZoomCamera);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_TurnPlayer, ETriggerEvent::Triggered, this, &AInputModule::TurnPlayer);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_MoveHPlayer, ETriggerEvent::Triggered, this, &AInputModule::MoveHPlayer);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_MoveVPlayer, ETriggerEvent::Triggered, this, &AInputModule::MoveVPlayer);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_MoveForwardPlayer, ETriggerEvent::Triggered, this, &AInputModule::MoveForwardPlayer);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_MoveRightPlayer, ETriggerEvent::Triggered, this, &AInputModule::MoveRightPlayer);
	InInputComponent->BindInputAction(InInputConfig, GameplayTags::InputTag_MoveUpPlayer, ETriggerEvent::Triggered, this, &AInputModule::MoveUpPlayer);
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

		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetPlayerController()->GetLocalPlayer()))
		{
			InputSubsystem->SetGamepadInputType(ControllerPlatform);
		}
	}
}

const UInputActionBase* AInputModule::FindInputActionForTag(const FGameplayTag& InInputTag, const UPlayerMappableInputConfig* InConfig, bool bLogNotFound) const
{
	if (InConfig)
	{
		for(const auto Iter1 : InConfig->GetMappingContexts())
		{
			for(auto Iter2 : Iter1.Key->GetMappings())
			{
				if(const auto InputAction = Cast<UInputActionBase>(Iter2.Action))
				{
					if(InputAction->ActionTag == InInputTag)
					{
						return InputAction;
					}
				}
			}
		}
	}
	for (const auto& Iter1 : RegisteredConfigMapping)
	{
		for(const auto Iter2 : Iter1.Config->GetMappingContexts())
		{
			for(auto Iter3 : Iter2.Key->GetMappings())
			{
				if(const auto InputAction = Cast<UInputActionBase>(Iter3.Action))
				{
					if(InputAction->ActionTag == InInputTag)
					{
						return InputAction;
					}
				}
			}
		}
	}
	
	if (bLogNotFound)
	{
		WHLog(FString::Printf(TEXT("Can't find InputAction for InputTag [%s] on InputConfig [%s]."), *InInputTag.ToString(), *GetNameSafe(this)), EDC_Input, EDV_Warning);
	}

	return nullptr;
}

void AInputModule::RegisterInputConfig(ECommonInputType InType, const UPlayerMappableInputConfig* InConfig, const bool bIsActive)
{
	if (InConfig)
	{
		const int32 Index = RegisteredConfigMapping.IndexOfByPredicate( [&InConfig](const FLoadedInputConfigMapping& Pair) { return Pair.Config == InConfig; } );
		if (Index == INDEX_NONE)
		{
			RegisteredConfigMapping.Add(FLoadedInputConfigMapping(InConfig, InType, bIsActive));
		}
	}
}

int32 AInputModule::UnregisterInputConfig(const UPlayerMappableInputConfig* InConfig)
{
	if (InConfig)
	{
		const int32 Index = RegisteredConfigMapping.IndexOfByPredicate( [&InConfig](const FLoadedInputConfigMapping& Pair) { return Pair.Config == InConfig; } );
		if (Index != INDEX_NONE)
		{
			RegisteredConfigMapping.RemoveAt(Index);
			return 1;
		}
			
	}
	return INDEX_NONE;
}

const UPlayerMappableInputConfig* AInputModule::GetInputConfigByName(FName InConfigName) const
{
	for (const auto& Iter : RegisteredConfigMapping)
	{
		if (Iter.Config->GetConfigName() == InConfigName)
		{
			return Iter.Config;
		}
	}
	return nullptr;
}

TArray<FLoadedInputConfigMapping> AInputModule::GetRegisteredConfigMappingOfType(ECommonInputType InType)
{
	TArray<FLoadedInputConfigMapping> ReturnValues;

	if (InType == ECommonInputType::Count)
	{
		ReturnValues = RegisteredConfigMapping;
	}
	else
	{
		for (const auto& Iter : RegisteredConfigMapping)
		{
			if (Iter.Type == InType)
			{
				ReturnValues.Emplace(Iter);
			}
		}
	}
	return ReturnValues;
}

TArray<FEnhancedActionKeyMapping> AInputModule::GetAllPlayerMappableActionKeyMappings()
{
	TArray<FEnhancedActionKeyMapping> ReturnValues;
	for (const auto& Iter1 : RegisteredConfigMapping)
	{
		if (Iter1.Type == ECommonInputType::MouseAndKeyboard)
		{
			for(auto Iter2 : Iter1.Config->GetPlayerMappableKeys())
			{
				if (CustomKeyMappings.Contains(Iter2.GetMappingName()))
				{
					Iter2.Key = CustomKeyMappings[Iter2.GetMappingName()];
				}
				ReturnValues.Add(Iter2);
			}
		}
	}
	return ReturnValues;
}

TArray<FName> AInputModule::GetAllActionMappingNamesFromKey(const FKey InKey, int32 InPlayerID)
{
	TArray<FName> ReturnValues;
	for (const auto& Iter2 : RegisteredConfigMapping)
	{
		if (Iter2.Type == ECommonInputType::MouseAndKeyboard)
		{
			for (const auto& Mapping : Iter2.Config->GetPlayerMappableKeys())
			{
				FName MappingName(Mapping.PlayerMappableOptions.DisplayName.ToString());
				FName ActionName = Mapping.PlayerMappableOptions.Name;
				
				if (const FKey* MappingKey = CustomKeyMappings.Find(ActionName))
				{
					if (*MappingKey == InKey)
					{
						ReturnValues.Add(MappingName);
					}
				}
				else
				{
					if (Mapping.Key == InKey)
					{
						ReturnValues.Add(MappingName);
					}
				}
			}
		}
	}
	return ReturnValues;
}

TArray<FEnhancedActionKeyMapping> AInputModule::GetAllActionMappingByName(const FName InName, int32 InPlayerID)
{
	TArray<FEnhancedActionKeyMapping> ReturnValues;
	for (const auto& Iter1 : RegisteredConfigMapping)
	{
		if (Iter1.Type == ECommonInputType::MouseAndKeyboard)
		{
			for (auto Iter2 : Iter1.Config->GetPlayerMappableKeys())
			{
				if(Iter2.GetMappingName() == InName)
				{
					if (CustomKeyMappings.Contains(Iter2.GetMappingName()))
					{
						Iter2.Key = CustomKeyMappings[Iter2.GetMappingName()];
					}
					ReturnValues.Add(Iter2);
				}
			}
		}
	}
	return ReturnValues;
}

TArray<FEnhancedActionKeyMapping> AInputModule::GetAllActionMappingByDisplayName(const FText InDisplayName, int32 InPlayerID)
{
	TArray<FEnhancedActionKeyMapping> ReturnValues;
	for (const FLoadedInputConfigMapping& Iter1 : RegisteredConfigMapping)
	{
		if (Iter1.Type == ECommonInputType::MouseAndKeyboard)
		{
			for (auto Iter2 : Iter1.Config->GetPlayerMappableKeys())
			{
				if(Iter2.PlayerMappableOptions.DisplayName.EqualTo(InDisplayName))
				{
					if (CustomKeyMappings.Contains(Iter2.GetMappingName()))
					{
						Iter2.Key = CustomKeyMappings[Iter2.GetMappingName()];
					}
					ReturnValues.Add(Iter2);
				}
			}
		}
	}
	return ReturnValues;
}

void AInputModule::AddOrUpdateCustomKeyboardBindings(const FName InName, const FKey InKey, int32 InPlayerID)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(UCommonBPLibrary::GetLocalPlayer(InPlayerID)))
	{
		Subsystem->AddPlayerMappedKeyInSlot(InName, InKey);
		CustomKeyMappings.Emplace(InName, InKey);
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
