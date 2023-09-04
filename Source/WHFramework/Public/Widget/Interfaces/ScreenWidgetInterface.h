// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BaseWidgetInterface.h"

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "UObject/Interface.h"
#include "ScreenWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UScreenWidgetInterface : public UBaseWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IScreenWidgetInterface : public IBaseWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCreate(UObject* InOwner = nullptr) = 0;

	virtual void OnInitialize(UObject* InOwner = nullptr) = 0;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) = 0;
	
	virtual void OnClose(bool bInstant = false) = 0;

	virtual void OnReset() = 0;

	virtual void OnStateChanged(EScreenWidgetState InWidgetChange) = 0;

public:
	virtual void Initialize(UObject* InOwner) = 0;

	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) = 0;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false) = 0;

	virtual void Close(bool bInstant = false) = 0;

	virtual void Toggle(bool bInstant = false) = 0;

	virtual void Reset() = 0;

protected:
	virtual void FinishOpen(bool bInstant) = 0;

	virtual void FinishClose(bool bInstant) = 0;

public:
	virtual void AddChild(IScreenWidgetInterface* InChildWidget) = 0;

	virtual void RemoveChild(IScreenWidgetInterface* InChildWidget) = 0;

	virtual void RemoveAllChild() = 0;

public:
	virtual FName GetParentName() const = 0;

	virtual TArray<FName> GetChildNames() const = 0;

	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const = 0;

	virtual EWidgetCreateType GetWidgetCreateType() const = 0;

	virtual EWidgetOpenType GetWidgetOpenType() const = 0;

	virtual EWidgetCloseType GetWidgetCloseType() const = 0;

	virtual EScreenWidgetState GetWidgetState() const = 0;

	virtual IScreenWidgetInterface* GetLastTemporary() const = 0;

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) = 0;

	virtual IScreenWidgetInterface* GetParentWidgetN() const = 0;

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) = 0;
	
	virtual IScreenWidgetInterface* GetTemporaryChild() const = 0;
	
	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) = 0;

	virtual int32 GetChildNum() const = 0;

	virtual TArray<IScreenWidgetInterface*>& GetChildWidgets() = 0;

	virtual IScreenWidgetInterface* GetChild(int32 InIndex) const = 0;
};
