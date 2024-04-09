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
	// InputMode
public:
	virtual void UpdateInputMode();

protected:
	EInputMode NativeInputMode;

	EInputMode GlobalInputMode;
	
public:
	virtual bool SetGlobalInputMode(EInputMode InInputMode);

	virtual EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	virtual int32 GetNativeInputPriority() const override { return 0; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};
