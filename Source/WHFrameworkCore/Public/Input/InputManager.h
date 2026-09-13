// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnGlobalInputModeChanged,
	EInputMode,
	EInputMode);

class WHFRAMEWORKCORE_API FInputManager : public FManagerBase
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
	virtual void UpdateInputMode();

	virtual void RequestInputMode(const void* InOwner, EInputMode InInputMode, int32 InPriority = 0);

	virtual void ReleaseInputMode(const void* InOwner);

	virtual void SetCommonUIInputMode(TOptional<EInputMode> InInputMode);

protected:
	struct FInputModeRequest
	{
		EInputMode InputMode = EInputMode::None;
		int32 Priority = 0;
	};

	EInputMode DefaultInputMode;
	
	EInputMode GlobalInputMode;

	TOptional<EInputMode> CommonUIInputMode;

	bool bCommonUIControlled;

	TMap<const void*, FInputModeRequest> InputModeRequests;

public:
	virtual EInputMode GetDefaultInputMode() const { return DefaultInputMode; }

	virtual void SetDefaultInputMode(EInputMode InInputMode);

	virtual EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	FOnGlobalInputModeChanged OnInputModeChanged;
};
