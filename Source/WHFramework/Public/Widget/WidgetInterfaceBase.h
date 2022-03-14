// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WidgetModuleTypes.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "UObject/Interface.h"
#include "WidgetInterfaceBase.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWidgetInterfaceBase : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IWidgetInterfaceBase
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	void TickWidget();

	UFUNCTION(BlueprintNativeEvent)
	bool IsTickAble() const;

public:
	virtual void OnRefresh() = 0;

	virtual void OnDestroy() = 0;

public:
	virtual void Refresh() = 0;

	virtual void Destroy() = 0;

public:
	virtual FName GetWidgetName() const = 0;

	virtual int32 GetWidgetZOrder() const = 0;

	virtual EWidgetRefreshType GetWidgetRefreshType() const = 0;

	virtual EInputMode GetInputMode() const = 0;

	virtual AActor* GetOwnerActor() const = 0;

	virtual class UPanelWidget* GetRootPanelWidget() const = 0;
};