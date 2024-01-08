// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleStatics.h"
#include "Main/Base/ModuleBase.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Input/EventHandle_InputModeChanged.h"
#include "Gameplay/WHPlayerController.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Input/InputManager.h"
#include "Common/CommonStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Base/InputActionBase.h"
#include "SaveGame/Module/InputSaveGame.h"
#include "Debug/DebugModuleTypes.h"
#include "Input/InputModuleStatics.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"
#include "Input/Widget/WidgetKeyTipsItemBase.h"

IMPLEMENTATION_MODULE(UInputModule)

// Sets default values
UInputModule::UInputModule()
{
	ModuleName = FName("InputModule");
	ModuleDisplayName = FText::FromString(TEXT("Input Module"));

	ModuleSaveGame = UInputSaveGame::StaticClass();
	
	AddKeyShortcut(GameplayTags::InputTag_InteractSelect, FInputKeyShortcut(FText::FromString("Interact Select"), FText::FromString("Interaction")));
	
	AddKeyShortcut(GameplayTags::InputTag_CameraPanMove, FInputKeyShortcut(FText::FromString("Camera Pan Move"), FText::FromString("Camera Control")));
	AddKeyShortcut(GameplayTags::InputTag_CameraRotate, FInputKeyShortcut(FText::FromString("Camera Rotate"), FText::FromString("Camera Control")));
	AddKeyShortcut(GameplayTags::InputTag_CameraZoom, FInputKeyShortcut(FText::FromString("Camera Zoom"), FText::FromString("Camera Control")));
	AddKeyShortcut(GameplayTags::InputTag_CameraSprint, FInputKeyShortcut(FText::FromString("Camera Sprint"), FText::FromString("Camera Control")));

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CameraMovementMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/DataAssets/IMC_CameraMovement.IMC_CameraMovement'"));
	if(CameraMovementMapping.Succeeded())
	{
		FInputContextMapping ContextMapping;
		ContextMapping.InputMapping = CameraMovementMapping.Object;
		ContextMappings.Add(ContextMapping);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CharacterMovementMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/DataAssets/IMC_CharacterMovement.IMC_CharacterMovement'"));
	if(CharacterMovementMapping.Succeeded())
	{
		FInputContextMapping ContextMapping;
		ContextMapping.InputMapping = CharacterMovementMapping.Object;
		ContextMappings.Add(ContextMapping);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CharacterActionMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/DataAssets/IMC_CharacterAction.IMC_CharacterAction'"));
	if(CharacterActionMapping.Succeeded())
	{
		FInputContextMapping ContextMapping;
		ContextMapping.InputMapping = CharacterActionMapping.Object;
		ContextMappings.Add(ContextMapping);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> SystemActionMapping(TEXT("/Script/EnhancedInput.InputMappingContext'/WHFramework/Input/DataAssets/IMC_SystemAction.IMC_SystemAction'"));
	if(SystemActionMapping.Succeeded())
	{
		FInputContextMapping ContextMapping;
		ContextMapping.InputMapping = SystemActionMapping.Object;
		ContextMappings.Add(ContextMapping);
	}

	AddTouchMapping(FInputTouchMapping(IE_Pressed, FInputTouchHandlerSignature::CreateUObject(this, &UInputModule::TouchPressed)));
	AddTouchMapping(FInputTouchMapping(IE_Released, FInputTouchHandlerSignature::CreateUObject(this, &UInputModule::TouchReleased)));
	AddTouchMapping(FInputTouchMapping(IE_Repeat, FInputTouchHandlerSignature::CreateUObject(this, &UInputModule::TouchMoved)));

	TouchInputRate = 1.f;
	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

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
}

void UInputModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		ApplyKeyMappings();
		ApplyTouchMappings();

		UEnhancedInputLocalPlayerSubsystem* Subsystem = UInputModuleStatics::GetInputSubsystem();
		UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>();
		UInputComponentBase* Component = UInputModuleStatics::GetInputComponent<UInputComponentBase>();

		for (const auto& Iter : ContextMappings)
		{
			if (Iter.bRegisterWithSettings)
			{
				Settings->RegisterInputMappingContext(Iter.InputMapping);
				FModifyContextOptions Options = {};
				Options.bIgnoreAllPressedKeysUntilRelease = false;
				Subsystem->AddMappingContext(Iter.InputMapping, Iter.Priority, Options);
			}
		}
		OnBindAction(Component);
		
		UpdateInputMode();
	}
}

void UInputModule::OnReset()
{
	Super::OnReset();

	TouchPressedCount = 0;
}

void UInputModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UInputModule::OnPause()
{
	Super::OnPause();

	GetPlayerController()->DisableInput(nullptr);
}

void UInputModule::OnUnPause()
{
	Super::OnUnPause();

	GetPlayerController()->EnableInput(nullptr);
}

void UInputModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UInputModule::OnBindAction(UInputComponentBase* InInputComponent)
{
	K2_OnBindAction(InInputComponent);

	InInputComponent->BindInputAction(GameplayTags::InputTag_TurnCamera, ETriggerEvent::Triggered, this, &UInputModule::TurnCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_LookUpCamera, ETriggerEvent::Triggered, this, &UInputModule::LookUpCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_PanHCamera, ETriggerEvent::Triggered, this, &UInputModule::PanHCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_PanVCamera, ETriggerEvent::Triggered, this, &UInputModule::PanVCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_ZoomCamera, ETriggerEvent::Triggered, this, &UInputModule::ZoomCamera);
	
	InInputComponent->BindInputAction(GameplayTags::InputTag_TurnPlayer, ETriggerEvent::Triggered, this, &UInputModule::TurnPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveHPlayer, ETriggerEvent::Triggered, this, &UInputModule::MoveHPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveVPlayer, ETriggerEvent::Triggered, this, &UInputModule::MoveVPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveForwardPlayer, ETriggerEvent::Triggered, this, &UInputModule::MoveForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveRightPlayer, ETriggerEvent::Triggered, this, &UInputModule::MoveRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveUpPlayer, ETriggerEvent::Triggered, this, &UInputModule::MoveUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_JumpPlayer, ETriggerEvent::Started, this, &UInputModule::JumpPlayer);

	InInputComponent->BindInputAction(GameplayTags::InputTag_SystemOperation, ETriggerEvent::Started, this, &UInputModule::SystemOperation);
}

void UInputModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInputModuleSaveData>();
	if(SaveData.IsSaved())
	{
		KeyShortcuts = SaveData.KeyShortcuts;

		UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>();

		UCommonStatics::LoadObjectDataFromMemory(Settings, SaveData.GetDatas());

		for(auto& Iter : SaveData.KeyMappings)
		{
			if(KeyMappings.Contains(Iter.Key))
			{
				KeyMappings[Iter.Key].Key = Iter.Value;
			}
		}

		RefreshData();
	}
}

void UInputModule::UnloadData(EPhase InPhase)
{
	
}

void UInputModule::RefreshData()
{
	LocalSaveData = GetSaveDataRef<FInputModuleSaveData>(true);

	TArray<UUserWidget*> KeyTipsItems;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, KeyTipsItems, UWidgetKeyTipsItemBase::StaticClass(), false);
	for(auto Iter : KeyTipsItems)
	{
		if(UWidgetKeyTipsItemBase* KeyTipsItem = Cast<UWidgetKeyTipsItemBase>(Iter))
		{
			KeyTipsItem->Refresh();
		}
	}
}

FSaveData* UInputModule::ToData()
{
	static FInputModuleSaveData SaveData;
	SaveData = FInputModuleSaveData();

	SaveData.KeyShortcuts = KeyShortcuts;

	UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>();

	UCommonStatics::SaveObjectDataToMemory(Settings, SaveData.GetDatas());

	for(auto& Iter : KeyMappings)
	{
		if(SaveData.KeyMappings.Contains(Iter.Key))
		{
			SaveData.KeyMappings[Iter.Key] = Iter.Value.Key;
		}
	}

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

void UInputModule::AddKeyShortcut(const FGameplayTag& InTag, const FInputKeyShortcut& InKeyShortcut)
{
	if(!KeyShortcuts.Contains(InTag))
	{
		KeyShortcuts.Add(InTag, InKeyShortcut);
	}
}

void UInputModule::RemoveKeyShortcut(const FGameplayTag& InTag)
{
	if(KeyShortcuts.Contains(InTag))
	{
		KeyShortcuts.Remove(InTag);
	}
}

FInputKeyShortcut UInputModule::GetKeyShortcut(const FGameplayTag& InTag) const
{
	if(KeyShortcuts.Contains(InTag))
	{
		return KeyShortcuts[InTag];
	}
	return FInputKeyShortcut();
}

void UInputModule::AddKeyMapping(const FGameplayTag& InTag, const FInputKeyMapping& InKeyMapping)
{
	if(!KeyMappings.Contains(InTag))
	{
		KeyMappings.Add(InTag, InKeyMapping);
	}
}

void UInputModule::RemoveKeyMapping(const FGameplayTag& InTag)
{
	if(KeyMappings.Contains(InTag))
	{
		KeyMappings.Remove(InTag);
	}
}

void UInputModule::AddTouchMapping(const FInputTouchMapping& InTouchMapping)
{
	TouchMappings.Add(InTouchMapping);
}

void UInputModule::AddPlayerKeyMapping(const FName InName, const FKey InKey, int32 InSlot, int32 InPlayerIndex)
{
	UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>(InPlayerIndex);
	if (!InKey.IsGamepadKey())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = InName;
		Args.Slot = (EPlayerMappableKeySlot)InSlot;
		Args.NewKey = InKey;
	
		FGameplayTagContainer FailureReason;
		Settings->MapPlayerKey(Args, FailureReason);

		RefreshData();
	}
}

void UInputModule::ApplyKeyMappings()
{
	GetPlayerController()->InputComponent->KeyBindings.Empty();
	for(auto& Iter : KeyMappings)
	{
		FInputKeyBinding KB(FInputChord(Iter.Value.Key, false, false, false, false), Iter.Value.Event);
		KB.KeyDelegate.BindDelegate(Iter.Value.Delegate.IsBound() ? Iter.Value.Delegate.GetUObject() : Iter.Value.DynamicDelegate.GetUObject(), Iter.Value.Delegate.IsBound() ? Iter.Value.Delegate.TryGetBoundFunctionName() : Iter.Value.DynamicDelegate.GetFunctionName());
		GetPlayerController()->InputComponent->KeyBindings.Emplace(MoveTemp(KB));
	}
}

void UInputModule::ApplyTouchMappings()
{
	GetPlayerController()->InputComponent->TouchBindings.Empty();
	for(auto& Iter : TouchMappings)
	{
		FInputTouchBinding TB(Iter.Event);
		TB.TouchDelegate.BindDelegate(Iter.Delegate.IsBound() ? Iter.Delegate.GetUObject() : Iter.DynamicDelegate.GetUObject(), Iter.Delegate.IsBound() ? Iter.Delegate.TryGetBoundFunctionName() : Iter.DynamicDelegate.GetFunctionName());
		GetPlayerController()->InputComponent->TouchBindings.Emplace(MoveTemp(TB));
	}
}

UInputActionBase* UInputModule::GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured) const
{
	for (const auto& Iter1 : ContextMappings)
	{
		if(UInputMappingContext* IMC = Iter1.InputMapping)
		{
			for(auto Iter2 : IMC->GetMappings())
			{
				if(const auto InputAction = Cast<UInputActionBase>(Iter2.Action))
				{
					if(InputAction->ActionTag == InTag)
					{
						return InputAction;
					}
				}
			}
		}
	}
	
	if (bEnsured)
	{
		WHLog(FString::Printf(TEXT("Can't find InputAction for InputTag [%s] on InputConfig [%s]."), *InTag.ToString(), *GetNameSafe(this)), EDC_Input, EDV_Warning);
	}

	return nullptr;
}

TArray<FEnhancedActionKeyMapping> UInputModule::GetAllActionKeyMappings(int32 InPlayerIndex)
{
	TArray<FEnhancedActionKeyMapping> Mappings;
	for (const auto& Iter1 : ContextMappings)
	{
		for(auto& Iter2 : Iter1.InputMapping->GetMappings())
		{
			Mappings.Add(Iter2);
		}
	}
	return Mappings;
}

TArray<FName> UInputModule::GetAllActionKeyMappingNames(int32 InPlayerIndex)
{
	TArray<FName> MappingNames;
	for (const auto& Iter1 : ContextMappings)
	{
		for(auto& Iter2 : Iter1.InputMapping->GetMappings())
		{
			MappingNames.Add(Iter2.GetMappingName());
		}
	}
	return MappingNames;
}

TArray<FPlayerKeyMapping> UInputModule::GetAllPlayerKeyMappings(int32 InPlayerIndex)
{
	TArray<FPlayerKeyMapping> Mappings;
	const UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>(InPlayerIndex);
	for (auto& Iter1 : Settings->GetAllSavedKeyProfiles())
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

TArray<FPlayerKeyMapping> UInputModule::GetPlayerKeyMappingsByName(const FName InName, int32 InPlayerIndex)
{
	TArray<FPlayerKeyMapping> Mappings;
	const UInputUserSettingsBase* Settings = UInputModuleStatics::GetInputUserSettings<UInputUserSettingsBase>(InPlayerIndex);
	for (auto& Iter1 : Settings->GetAllSavedKeyProfiles())
	{
		if(Iter1.Value->GetPlayerMappingRows().Contains(InName))
		{
			for (const FPlayerKeyMapping& Iter2 : Iter1.Value->GetPlayerMappingRows()[InName].Mappings)
			{
				Mappings.Add(Iter2);
			}
		}
	}
	if(Mappings.Num() > 1)
	{
		Mappings.Sort([](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B){
			return A.GetSlot() < B.GetSlot();
		});
	}
	return Mappings;
}

bool UInputModule::IsPlayerMappedKeyByName(const FName InName, const FKey& InKey, int32 InPlayerIndex) const
{
	for(auto& Iter : UInputModuleStatics::GetPlayerKeyMappingsByName(FName("SystemOperation")))
	{
		if(InKey == Iter.GetCurrentKey())
		{
			return true;
		}
	}
	return false;
}

void UInputModule::TurnCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(GetKeyShortcut(GameplayTags::InputTag_CameraRotate).IsPressing(GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>(), 0.f);
	}
}

void UInputModule::LookUpCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(GetKeyShortcut(GameplayTags::InputTag_CameraRotate).IsPressing(GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(0.f, UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>());
	}
}

void UInputModule::PanHCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcut(GameplayTags::InputTag_CameraPanMove).IsPressing(GetPlayerController()))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -1.f : 1.f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void UInputModule::PanVCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcut(GameplayTags::InputTag_CameraPanMove).IsPressing(GetPlayerController()))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (UCameraModule::Get().IsReverseCameraPanMove() ? -1.f : 1.f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void UInputModule::ZoomCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetKeyShortcut(GameplayTags::InputTag_CameraZoom).IsPressing(GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>());
	}
}

void UInputModule::MoveForwardCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(GetPlayerController()) ? 3.f : 1.5f));
}

void UInputModule::MoveRightCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(GetPlayerController()) ? 3.f : 1.5f));
}

void UInputModule::MoveUpCamera(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() * (GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(GetPlayerController()) ? 3.f : 1.5f));
}

void UInputModule::TurnPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UInputModule::MoveHPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveH(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UInputModule::MoveVPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveV(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UInputModule::MoveForwardPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveForwardCamera(InValue);
	}
}

void UInputModule::MoveRightPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveRightCamera(InValue);
	}
}

void UInputModule::MoveUpPlayer(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveUpCamera(InValue);
	}
}

void UInputModule::JumpPlayer()
{
	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(GetPlayerController()->GetPawn());
	}
}

void UInputModule::SystemOperation()
{
}

void UInputModule::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
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

void UInputModule::TouchPressedImpl()
{
	TouchPressedCount++;

	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

void UInputModule::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &UInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle1, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch2:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &UInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle2, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch3:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &UInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle3, TimerDelegate, 0.15f, false);
			break;
		}
		default: break;
	}
}

void UInputModule::TouchReleasedImpl(ETouchIndex::Type InTouchIndex)
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

void UInputModule::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
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
			UCameraModule::Get().AddCameraRotationInput((TouchLocationX - TouchLocationPrevious.X) * TouchInputRate, -(TouchLocationY - TouchLocationPrevious.Y) * TouchInputRate);
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
			UCameraModuleStatics::AddCameraDistanceInput(-(TouchCurrentPinchValue - TouchPinchValuePrevious) * TouchInputRate);
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
			UCameraModule::Get().AddCameraMovementInput(DirectionH + DirectionV, TouchInputRate * (UCameraModule::Get().IsReverseCameraPanMove() ? -1.f : 1.f));
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
}

AWHPlayerController* UInputModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>();
	}
	return PlayerController;
}

void UInputModule::UpdateInputMode()
{
	TArray<IInputManager*> InputManagers = AMainModule::GetAllModule<IInputManager>();
	// InputManagers.Sort([](const IInputManager& A, const IInputManager& B){
	// 	return A.GetNativeInputPriority() < B.GetNativeInputPriority();
	// });
	EInputMode InputMode = EInputMode::None;
	for (const auto Iter : InputManagers)
	{
		if ((int32)Iter->GetNativeInputMode() > (int32)InputMode)
		{
			InputMode = Iter->GetNativeInputMode();
		}
	}
	SetGlobalInputMode(InputMode);
}

void UInputModule::SetGlobalInputMode(EInputMode InInputMode)
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
			case EInputMode::GameOnly_NotHideCursor:
			{
				GetPlayerController()->SetInputMode(FInputModeGameOnly_NotHideCursor());
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
			case EInputMode::UIOnly:
			{
				GetPlayerController()->SetInputMode(FInputModeUIOnly());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			default: break;
		}
		UEventModuleStatics::BroadcastEvent(UEventHandle_InputModeChanged::StaticClass(), this, { &GlobalInputMode }, EEventNetType::Multicast);
	}
}
