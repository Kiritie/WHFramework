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
	// ManagerBase
public:
	virtual void OnInitialize() override;

	virtual void OnPreparatory() override;

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnTermination() override;

	//////////////////////////////////////////////////////////////////////////
	// InputManager
public:
	virtual void AddInputManager(IInputManagerInterface* InInputManager);

	virtual void RemoveInputManager(IInputManagerInterface* InInputManager);

	virtual void UpdateInputMode();

protected:
	EInputMode NativeInputMode;
	
	EInputMode GlobalInputMode;
	
	TArray<IInputManagerInterface*> InputManagers;

public:
	virtual int32 GetNativeInputPriority() const override { return 0; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }

	virtual void SetNativeInputMode(EInputMode InInputMode) override;

	virtual EInputMode GetGlobalInputMode() const { return GlobalInputMode; }
};
