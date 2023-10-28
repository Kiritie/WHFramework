// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/Components/InputComponentBase.h"

#include "EnhancedInputSubsystems.h"
#include "PlayerMappableInputConfig.h"
#include "Input/InputModule.h"

UInputComponentBase::UInputComponentBase(const FObjectInitializer& ObjectInitializer)
{
}

void UInputComponentBase::AddInputMappings(const UPlayerMappableInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	ULocalPlayer* LocalPlayer = InputSubsystem->GetLocalPlayer<ULocalPlayer>();
	check(LocalPlayer);

	// Add any registered input mappings from the settings!
	if (AInputModule* LocalSettings = AInputModule::Get())
	{	
		// Tell enhanced input about any custom keymappings that the player may have customized
		for (const TPair<FName, FKey>& Pair : LocalSettings->GetCustomPlayerInputConfig())
		{
			if (Pair.Key != NAME_None && Pair.Value.IsValid())
			{
				InputSubsystem->AddPlayerMappedKeyInSlot(Pair.Key, Pair.Value);
			}
		}
	}
}

void UInputComponentBase::RemoveInputMappings(const UPlayerMappableInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	ULocalPlayer* LocalPlayer = InputSubsystem->GetLocalPlayer<ULocalPlayer>();
	check(LocalPlayer);
	
	if (AInputModule* LocalSettings = AInputModule::Get())
	{
		// Remove any registered input contexts
		const TArray<FLoadedInputConfigMapping>& Configs = LocalSettings->GetAllRegisteredInputConfigs();
		for (const FLoadedInputConfigMapping& Pair : Configs)
		{
			InputSubsystem->RemovePlayerMappableConfig(Pair.Config);
		}
		
		// Clear any player mapped keys from enhanced input
		for (const TPair<FName, FKey>& Pair : LocalSettings->GetCustomPlayerInputConfig())
		{
			InputSubsystem->RemovePlayerMappedKeyInSlot(Pair.Key);
		}
	}
}

template<class UserClass, typename FuncType>
void UInputComponentBase::BindNativeAction(const UPlayerMappableInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputActionBase* IA = AInputModule::Get()->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(IA, TriggerEvent, Object, Func);
	}
}

void UInputComponentBase::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
