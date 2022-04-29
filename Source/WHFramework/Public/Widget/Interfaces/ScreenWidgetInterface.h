// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	virtual void OnCreate() = 0;

	virtual void OnInitialize(AActor* InOwner = nullptr) = 0;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) = 0;
	
	virtual void OnClose(bool bInstant = false) = 0;

	virtual void OnReset() = 0;

	virtual void OnStateChanged(EWidgetState InWidgetChange) = 0;

public:
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) = 0;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false) = 0;

	virtual void Close(bool bInstant = false) = 0;

	virtual void Toggle(bool bInstant = false) = 0;

	virtual void Reset() = 0;

	virtual void AddChild(const TScriptInterface<IScreenWidgetInterface>& InChildWidget) = 0;

	virtual void RemoveChild(const TScriptInterface<IScreenWidgetInterface>& InChildWidget) = 0;

	virtual void RemoveAllChild(const TScriptInterface<IScreenWidgetInterface>& InChildWidget) = 0;

	virtual void RefreshAllChild() = 0;

protected:
	virtual void FinishOpen(bool bInstant) = 0;

	virtual void FinishClose(bool bInstant) = 0;

public:
	virtual FName GetParentName() const = 0;

	virtual EWidgetType GetWidgetType() const = 0;

	virtual EWidgetCategory GetWidgetCategory() const = 0;

	virtual EWidgetCreateType GetWidgetCreateType() const = 0;

	virtual EWidgetOpenType GetWidgetOpenType() const = 0;

	virtual EWidgetCloseType GetWidgetCloseType() const = 0;

	virtual EWidgetState GetWidgetState() const = 0;

	virtual TScriptInterface<IScreenWidgetInterface> GetLastWidget() const = 0;

	virtual void SetLastWidget(TScriptInterface<IScreenWidgetInterface> InLastWidget) = 0;

	virtual TScriptInterface<IScreenWidgetInterface> GetParentWidget() const = 0;

	virtual void SetParentWidget(TScriptInterface<IScreenWidgetInterface> InParentWidget) = 0;

	virtual TArray<TScriptInterface<IScreenWidgetInterface>>& GetChildWidgets() = 0;
};
