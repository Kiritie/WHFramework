// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputManagerInterface.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FInputManager : public FManagerBase, public IInputManagerInterface
{
	GENERATED_MANAGER(FInputManager)

public:	
	FInputManager();

	virtual ~FInputManager() override;
	
	static const FUniqueType Type;

	//////////////////////////////////////////////////////////////////////////
	// InputManager
public:
	virtual void UpdateInputMode();

protected:
	EInputMode _NativeInputMode;
	EInputMode _GlobalInputMode;

public:
	virtual int32 GetNativeInputPriority() const override { return 0; }

	virtual EInputMode GetNativeInputMode() const override { return _NativeInputMode; }

	virtual EInputMode GetGlobalInputMode() const { return _GlobalInputMode; }
};
