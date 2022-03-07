// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WidgetModuleTypes.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "UObject/Interface.h"
#include "WidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCreate() = 0;

	virtual void OnInitialize(AActor* InOwner = nullptr) = 0;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) = 0;
	
	virtual void OnClose(bool bInstant = false) = 0;

	virtual void OnReset() = 0;

	virtual void OnRefresh() = 0;

	virtual void OnDestroy() = 0;

public:
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) = 0;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false) = 0;

	virtual void Close(bool bInstant = false) = 0;

	virtual void Toggle(bool bInstant = false) = 0;

	virtual void Reset() = 0;

	virtual void Refresh() = 0;

	virtual void Destroy() = 0;

protected:
	virtual void FinishOpen(bool bInstant) = 0;

	virtual void FinishClose(bool bInstant) = 0;

public:
	virtual FName GetWidgetName() const = 0;

	virtual FName GetParentName() const = 0;

	virtual int32 GetWidgetZOrder() const = 0;

	virtual EWidgetType GetWidgetType() const = 0;

	virtual EWidgetOpenType GetWidgetOpenType() const = 0;

	virtual EWidgetCloseType GetWidgetCloseType() const = 0;

	virtual EWidgetRefreshType GetWidgetRefreshType() const = 0;

	virtual EWidgetState GetWidgetState() const = 0;

	virtual EInputMode GetInputMode() const = 0;

	virtual AActor* GetOwnerActor() const = 0;

	virtual class UPanelWidget* GetRootPanelWidget() const = 0;

	virtual TScriptInterface<IWidgetInterface> GetLastWidget() const = 0;

	virtual void SetLastWidget(TScriptInterface<IWidgetInterface> InLastWidget) = 0;

	virtual TScriptInterface<IWidgetInterface> GetParentWidget() const = 0;

	virtual void SetParentWidget(TScriptInterface<IWidgetInterface> InParentWidget) = 0;

	virtual TArray<TScriptInterface<IWidgetInterface>>& GetChildWidgets() = 0;

	virtual void SetChildWidgets(const TArray<TScriptInterface<IWidgetInterface>>& InChildWidgets) = 0;
};
