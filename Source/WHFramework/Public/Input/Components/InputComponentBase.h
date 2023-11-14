// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedInputComponent.h"
#include "Input/InputModule.h"
#include "Input/Base/InputActionBase.h"
#include "InputMappingContext.h"

#include "InputComponentBase.generated.h"

/**
 * UInputComponentBase
 *
 *	Component used to manage input mappings and bindings using an input config data asset.
 */
UCLASS(Config = Input)
class WHFRAMEWORK_API UInputComponentBase : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UInputComponentBase(const FObjectInitializer& ObjectInitializer);

public:
	template<class UserClass, typename FuncType>
	void BindInputAction(UInputMappingContext* InInputContext, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound = true)
	{
		check(InInputContext);
		if (const UInputActionBase* InputAction = UInputModule::Get().FindInputActionForTag(InputTag, InInputContext, bLogIfNotFound))
		{
			BindAction(InputAction, TriggerEvent, Object, Func);
		}
	}
	
	void RemoveBinds(TArray<uint32>& BindHandles);
};
