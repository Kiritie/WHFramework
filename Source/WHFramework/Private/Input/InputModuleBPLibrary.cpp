// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleBPLibrary.h"

#include "Global/GlobalBPLibrary.h"
#include "Input/InputModule.h"
#include "Main/MainModule.h"

int32 UInputModuleBPLibrary::GetTouchPressedCount()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetTouchPressedCount();
	}
	return 0;
}

FInputKeyShortcut UInputModuleBPLibrary::GetKeyShortcutByName(const FName InName)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetKeyShortcutByName(InName);
	}
	return FInputKeyShortcut();
}

FEventReply UInputModuleBPLibrary::OnWidgetInputKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->OnWidgetInputKeyDown(InGeometry, InKeyEvent);
	}
	return FEventReply(false);
}

FEventReply UInputModuleBPLibrary::OnWidgetInputMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->OnWidgetInputMouseButtonDown(InGeometry, InMouseEvent);
	}
	return FEventReply(false);
}

EInputMode UInputModuleBPLibrary::GetGlobalInputMode()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetGlobalInputMode();
	}
	return EInputMode::None;
}

void UInputModuleBPLibrary::SetGlobalInputMode(EInputMode InInputMode)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		InputModule->SetGlobalInputMode(InInputMode);
	}
}

void UInputModuleBPLibrary::UpdateGlobalInputMode()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}
}

void UInputModuleBPLibrary::BindInputKey(const FKey Key, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		FInputKeyBinding KB(FInputChord(Key, false, false, false, false), KeyEvent);
		KB.KeyDelegate = FInputActionUnifiedDelegate(Delegate);
		PlayerController->InputComponent->KeyBindings.Emplace(MoveTemp(KB));
	}
}

void UInputModuleBPLibrary::BindInputAction(const FName ActionName, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		FInputActionBinding AB(ActionName, KeyEvent);
		AB.ActionDelegate = FInputActionUnifiedDelegate(Delegate);
		PlayerController->InputComponent->AddActionBinding(AB);
	}
}

void UInputModuleBPLibrary::BindInputAxis(const FName AxisName, const FInputAxisHandlerDynamicSignature& Delegate)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		FInputAxisBinding AB(AxisName);
		AB.AxisDelegate.BindDelegate(const_cast<UObject*>(Delegate.GetUObject()), Delegate.GetFunctionName());
		PlayerController->InputComponent->AxisBindings.Emplace(MoveTemp(AB));
	}
}

void UInputModuleBPLibrary::BindInputTouch(const EInputEvent KeyEvent, const FInputTouchHandlerDynamicSignature& Delegate)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		FInputTouchBinding TB(KeyEvent);
		TB.TouchDelegate.BindDelegate(const_cast<UObject*>(Delegate.GetUObject()), Delegate.GetFunctionName());
		PlayerController->InputComponent->TouchBindings.Emplace(MoveTemp(TB));
	}
}
