// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedInputComponent.h"
#include "Input/InputModule.h"
#include "Input/Base/InputActionBase.h"
#include "Input/Manager/InputBindingBase.h"
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
	uint32 BindInputAction(
		const FGameplayTag& InTag,
		ETriggerEvent InTriggerEvent,
		UserClass* InObject,
		FuncType InFunction,
		bool bEnsured = true)
	{
		if(const UInputActionBase* InputAction = UInputModule::Get().GetInputActionByTag(InTag, bEnsured))
		{
			FEnhancedInputActionEventBinding& Binding = BindAction(InputAction, InTriggerEvent, InObject, InFunction);
			const uint32 Handle = Binding.GetHandle();
			if(UInputBindingBase* InputBinding = Cast<UInputBindingBase>(InObject))
			{
				InputBinding->AddBindingHandle(Handle);
			}
			return Handle;
		}
		return 0;
	}
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	int32 BindInputAction(
		const FGameplayTag& InTag,
		ETriggerEvent InTriggerEvent,
		const FEnhancedInputActionHandlerDynamicSignature& InDelegate,
		bool bEnsured = true)
	{
		if(const UInputActionBase* InputAction = UInputModule::Get().GetInputActionByTag(InTag, bEnsured))
		{
			TUniquePtr<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>> AB =
				MakeUnique<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>>(
					InputAction,
					InTriggerEvent);
			AB->Delegate.BindDelegate(const_cast<UObject*>(InDelegate.GetUObject()), InDelegate.GetFunctionName());
			AB->Delegate.SetShouldFireWithEditorScriptGuard(ShouldFireDelegatesInEditor());
			const uint32 Handle = AB->GetHandle();
			const_cast<TArray<TUniquePtr<FEnhancedInputActionEventBinding>>&>(GetActionEventBindings()).Add(MoveTemp(AB));
			if(UInputBindingBase* InputBinding = Cast<UInputBindingBase>(const_cast<UObject*>(InDelegate.GetUObject())))
			{
				InputBinding->AddBindingHandle(Handle);
			}
			return static_cast<int32>(Handle);
		}
		return 0;
	}
	
	void RemoveBinds(TArray<uint32>& BindHandles);
};
