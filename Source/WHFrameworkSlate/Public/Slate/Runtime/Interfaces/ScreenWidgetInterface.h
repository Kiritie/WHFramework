// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PanelWidgetInterface.h"

#include "UObject/Interface.h"
#include "ScreenWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UScreenWidgetInterface : public UPanelWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API IScreenWidgetInterface : public IPanelWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) = 0;

	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) = 0;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) = 0;
	
	virtual void OnClose(bool bInstant = false) = 0;

	virtual void OnStateChanged(EScreenWidgetState InWidgetChange) = 0;

public:
	virtual void Init(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false) = 0;
	
	virtual void Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false) = 0;

	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false) = 0;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false) = 0;

	virtual void Close(bool bInstant = false) = 0;

	virtual void Toggle(bool bInstant = false) = 0;

public:
	virtual bool CanOpen() const = 0;

protected:
	virtual void FinishOpen(bool bInstant) = 0;

	virtual void FinishClose(bool bInstant) = 0;

public:
	virtual void AddChildWidget(IScreenWidgetInterface* InWidget) = 0;

	virtual void RemoveChildWidget(IScreenWidgetInterface* InWidget) = 0;

	virtual void RemoveAllChildWidget() = 0;

public:
	virtual FName GetParentName() const = 0;

	virtual FName GetParentSlot() const = 0;

	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const = 0;

	virtual EWidgetCreateType GetWidgetCreateType() const = 0;

	virtual EWidgetOpenType GetWidgetOpenType() const = 0;

	virtual EWidgetCloseType GetWidgetCloseType() const = 0;

	virtual EScreenWidgetState GetWidgetState(bool bInheritParent = false) const = 0;
	
	virtual bool IsWidgetOpened(bool bCheckOpening = true, bool bInheritParent = false) const = 0;
	
	virtual bool IsWidgetClosed(bool bCheckClosing = true, bool bInheritParent = false) const = 0;

	virtual IScreenWidgetInterface* GetLastTemporary() const = 0;

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) = 0;

	virtual IScreenWidgetInterface* GetParentWidgetN() const = 0;

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) = 0;

	virtual IScreenWidgetInterface* GetTemporaryChild() const = 0;
	
	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) = 0;

	virtual int32 GetChildWidgetNum() const = 0;

	virtual TArray<IScreenWidgetInterface*> GetChildWidgets() = 0;

	virtual IScreenWidgetInterface* GetChildWidget(int32 InIndex) const = 0;

	virtual int32 FindChildWidget(IScreenWidgetInterface* InWidget) const = 0;
};
