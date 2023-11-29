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
	void BindInputAction(const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bEnsured = true)
	{
		if (const UInputActionBase* InputAction = UInputModule::Get().FindInputActionForTag(InputTag, bEnsured))
		{
			BindAction(InputAction, TriggerEvent, Object, Func);
		}
	}
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InputTag"))
	void BindInputAction(const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, const FEnhancedInputActionHandlerDynamicSignature& Delegate, bool bEnsured = true)
	{
		if (const UInputActionBase* InputAction = UInputModule::Get().FindInputActionForTag(InputTag, bEnsured))
		{
			TUniquePtr<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>> AB = MakeUnique<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>>(InputAction, TriggerEvent);
			AB->Delegate.BindDelegate(const_cast<UObject*>(Delegate.GetUObject()), Delegate.GetFunctionName());
			AB->Delegate.SetShouldFireWithEditorScriptGuard(ShouldFireDelegatesInEditor());
			const_cast<TArray<TUniquePtr<FEnhancedInputActionEventBinding>>&>(GetActionEventBindings()).Add(MoveTemp(AB));
		}
	}
	
	void RemoveBinds(TArray<uint32>& BindHandles);
};
