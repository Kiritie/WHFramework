#include "Input/InputModuleStatics.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"
#include "Input/InputManager.h"
#include "Input/InputModule.h"

namespace
{
	UInputModule* FindPlayerInput(APlayerController* Player, int32& Index)
	{
		Index = INDEX_NONE;
		if (!::IsValid(Player) || !Player->IsLocalController()) return nullptr;
		UInputModule* Module = UInputModule::GetPtr(Player->GetWorld());
		if (Module) Index = Module->FindPlayerRuntimeIndex(Player);
		return Index != INDEX_NONE ? Module : nullptr;
	}
}

UEnhancedInputLocalPlayerSubsystem* UInputModuleStatics::GetInputSubsystem(APlayerController* Player)
{
	ULocalPlayer* Local = Player && Player->IsLocalController() ? Player->GetLocalPlayer() : nullptr;
	return Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

UInputUserSettingsBase* UInputModuleStatics::GetInputUserSettings(APlayerController* Player, TSubclassOf<UInputUserSettingsBase> Class)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(Player);
	UInputUserSettingsBase* Settings = Subsystem ? Subsystem->GetUserSettings<UInputUserSettingsBase>() : nullptr;
	return Settings && (!Class || Settings->IsA(Class)) ? Settings : nullptr;
}

UInputComponentBase* UInputModuleStatics::GetInputComponent(APlayerController* Player, TSubclassOf<UInputComponentBase> Class)
{
	UInputComponentBase* Component = Player && Player->IsLocalController() ? Cast<UInputComponentBase>(Player->InputComponent) : nullptr;
	return Component && (!Class || Component->IsA(Class)) ? Component : nullptr;
}

UInputBindingBase* UInputModuleStatics::GetInputBinding(APlayerController* Player, TSubclassOf<UInputBindingBase> Class)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetInputBinding(Class, Index) : nullptr;
}

UInputBindingBase* UInputModuleStatics::GetInputBindingByName(APlayerController* Player, FName Name, TSubclassOf<UInputBindingBase> Class)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetInputBindingByName(Name, Index, Class) : nullptr;
}

TArray<FEnhancedActionKeyMapping> UInputModuleStatics::GetAllActionKeyMappings(APlayerController* Player)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetAllActionKeyMappings(Index) : TArray<FEnhancedActionKeyMapping>();
}

TArray<FName> UInputModuleStatics::GetAllActionKeyMappingNames(APlayerController* Player)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetAllActionKeyMappingNames(Index) : TArray<FName>();
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetAllPlayerKeyMappings(APlayerController* Player)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetAllPlayerKeyMappings(Index) : TArray<FPlayerKeyMapping>();
}

bool UInputModuleStatics::IsPlayerMappedKeyByTag(APlayerController* Player, const FGameplayTag& Tag, const FKey& Key)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->IsPlayerMappedKeyByTag(Tag, Key, Index);
}

const UInputActionBase* UInputModuleStatics::GetInputActionByTag(UWorld* World, const FGameplayTag& Tag, bool bEnsured)
{
	const UInputModule* Module = UInputModule::GetPtr(World);
	return Module ? Module->GetInputActionByTag(Tag, bEnsured) : nullptr;
}

bool UInputModuleStatics::IsInputActionActive(APlayerController* Player, const FGameplayTag& Tag)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->IsInputActionActive(Tag, Index);
}

bool UInputModuleStatics::ActivateInputContext(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->ActivateInputContext(Tag, Index);
}

bool UInputModuleStatics::DeactivateInputContext(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->DeactivateInputContext(Tag, Index);
}

bool UInputModuleStatics::IsInputContextActive(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->IsInputContextActive(Tag, Index);
}

bool UInputModuleStatics::MapPlayerKeyByTag(APlayerController* Player, FGameplayTag Tag, FKey Key, EPlayerMappableKeySlot Slot, FGameplayTagContainer& Failure)
{
	Failure.Reset();
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->MapPlayerKeyByTag(Tag, Key, Slot, Index, &Failure);
}

bool UInputModuleStatics::MapPlayerKeyByMappingName(APlayerController* Player, FName Name, FKey Key, EPlayerMappableKeySlot Slot, FGameplayTagContainer& Failure)
{
	Failure.Reset();
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->MapPlayerKeyByMappingName(Name, Key, Slot, Index, &Failure);
}

bool UInputModuleStatics::ResetPlayerKeyByTag(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->ResetPlayerKeyByTag(Tag, Index);
}

bool UInputModuleStatics::ResetPlayerKeyByMappingName(APlayerController* Player, FName Name)
{
	int32 Index; UInputModule* Module = FindPlayerInput(Player, Index);
	return Module && Module->ResetPlayerKeyByMappingName(Name, Index);
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetPlayerKeyMappingsByTag(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetPlayerKeyMappingsByTag(Tag, Index) : TArray<FPlayerKeyMapping>();
}

TArray<FKey> UInputModuleStatics::GetKeysByActionTag(APlayerController* Player, FGameplayTag Tag)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetKeysByActionTag(Tag, Index) : TArray<FKey>();
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetPlayerKeyMappingsByMappingName(APlayerController* Player, FName Name)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetPlayerKeyMappingsByMappingName(Name, Index) : TArray<FPlayerKeyMapping>();
}

FText UInputModuleStatics::GetPlayerKeyCodeByTag(APlayerController* Player, FGameplayTag Tag)
{
	FString Text;
	for (const FPlayerKeyMapping& Mapping : GetPlayerKeyMappingsByTag(Player, Tag))
	{
		if (!Text.IsEmpty()) Text += TEXT("/");
		Text += Mapping.GetCurrentKey().GetDisplayName(false).ToString();
	}
	return FText::FromString(Text);
}

TArray<FGameplayTag> UInputModuleStatics::GetAllMappableActions(UWorld* World)
{
	const UInputModule* Module = UInputModule::GetPtr(World);
	return Module ? Module->GetAllMappableActions() : TArray<FGameplayTag>();
}

TArray<FInputMappableEntry> UInputModuleStatics::GetAllMappableEntries(UWorld* World)
{
	const UInputModule* Module = UInputModule::GetPtr(World);
	return Module ? Module->GetAllMappableEntries() : TArray<FInputMappableEntry>();
}

TArray<FInputMappableEntry> UInputModuleStatics::GetMappableEntriesByActionTag(UWorld* World, FGameplayTag Tag)
{
	const UInputModule* Module = UInputModule::GetPtr(World);
	return Module ? Module->GetMappableEntriesByActionTag(Tag) : TArray<FInputMappableEntry>();
}

ECommonInputType UInputModuleStatics::GetCurrentInputType(APlayerController* Player)
{
	int32 Index; const UInputModule* Module = FindPlayerInput(Player, Index);
	return Module ? Module->GetCurrentInputType(Index) : ECommonInputType::Count;
}

bool UInputModuleStatics::IsUsingGamepad(APlayerController* Player)
{
	return GetCurrentInputType(Player) == ECommonInputType::Gamepad;
}

bool UInputModuleStatics::IsUsingMouseAndKeyboard(APlayerController* Player)
{
	return GetCurrentInputType(Player) == ECommonInputType::MouseAndKeyboard;
}

bool UInputModuleStatics::IsUsingTouch(APlayerController* Player)
{
	return GetCurrentInputType(Player) == ECommonInputType::Touch;
}

EInputMode UInputModuleStatics::GetDefaultInputMode(const UObject* Context)
{
	const UInputModule* Module = UInputModule::GetPtr(Context ? Context->GetWorld() : nullptr);
	return Module ? Module->GetDefaultInputMode() : EInputMode::None;
}

void UInputModuleStatics::SetDefaultInputMode(const UObject* Context, EInputMode Mode)
{
	if (UInputModule* Module = UInputModule::GetPtr(Context ? Context->GetWorld() : nullptr)) Module->SetDefaultInputMode(Mode);
}

EInputMode UInputModuleStatics::GetGlobalInputMode(APlayerController* Player)
{
	return Player && Player->IsLocalController() ? FInputManager::Get().GetGlobalInputMode(Player) : EInputMode::None;
}
