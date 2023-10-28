// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedInputComponent.h"

#include "InputComponentBase.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UPlayerMappableInputConfig;


/**
 * UInputComponentBase
 *
 *	Component used to manage input mappings and bindings using an input config data asset.
 */
UCLASS(Config = Input)
class UInputComponentBase : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UInputComponentBase(const FObjectInitializer& ObjectInitializer);

public:
	void AddInputMappings(const UPlayerMappableInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const;

	void RemoveInputMappings(const UPlayerMappableInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const;

	template<class UserClass, typename FuncType>
	void BindNativeAction(const UPlayerMappableInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound = true);

	void RemoveBinds(TArray<uint32>& BindHandles);
};
