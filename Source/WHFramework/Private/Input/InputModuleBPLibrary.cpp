// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleBPLibrary.h"

#include "Global/GlobalBPLibrary.h"
#include "Input/InputModule.h"
#include "Main/MainModule.h"

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

bool UInputModuleBPLibrary::IsInputKeyDown(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->IsInputKeyDown(Key);
	}
	return false;
}

bool UInputModuleBPLibrary::WasInputKeyJustPressed(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->WasInputKeyJustPressed(Key);
	}
	return false;
}

bool UInputModuleBPLibrary::WasInputKeyJustReleased(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->WasInputKeyJustReleased(Key);
	}
	return false;
}

float UInputModuleBPLibrary::GetInputAnalogKeyState(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetInputAnalogKeyState(Key);
	}
	return 0.f;
}

FVector UInputModuleBPLibrary::GetInputVectorKeyState(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetInputVectorKeyState(Key);
	}
	return FVector::ZeroVector;
}

void UInputModuleBPLibrary::GetInputTouchState(ETouchIndex::Type FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->GetInputTouchState(FingerIndex, LocationX, LocationY, bIsCurrentlyPressed);
	}
}

void UInputModuleBPLibrary::GetInputMotionState(FVector& Tilt, FVector& RotationRate, FVector& Gravity, FVector& Acceleration)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->GetInputMotionState(Tilt, RotationRate, Gravity, Acceleration);
	}
}

bool UInputModuleBPLibrary::GetShowMouseCursor()
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->ShouldShowMouseCursor();
	}
	return false;
}

void UInputModuleBPLibrary::SetShowMouseCursor(bool bShow)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->SetShowMouseCursor(bShow);
	}
}

bool UInputModuleBPLibrary::GetMousePosition(float& LocationX, float& LocationY)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetMousePosition(LocationX, LocationY);
	}
	return false;
}

void UInputModuleBPLibrary::SetMouseLocation(const int X, const int Y)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->SetMouseLocation(X, Y);
	}
}

float UInputModuleBPLibrary::GetInputKeyTimeDown(FKey Key)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetInputKeyTimeDown(Key);
	}
	return false;
}

void UInputModuleBPLibrary::GetInputMouseDelta(float& DeltaX, float& DeltaY)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->GetInputMouseDelta(DeltaX, DeltaY);
	}
}

void UInputModuleBPLibrary::GetInputAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->GetInputAnalogStickState(WhichStick, StickX, StickY);
	}
}

void UInputModuleBPLibrary::ActivateTouchInterface(UTouchInterface* NewTouchInterface)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		return PlayerController->ActivateTouchInterface(NewTouchInterface);
	}
}

void UInputModuleBPLibrary::SetVirtualJoystickVisibility(bool bVisible)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		PlayerController->SetVirtualJoystickVisibility(bVisible);
	}
}
