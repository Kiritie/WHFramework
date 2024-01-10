// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BaseWidgetInterface.h"

#include "Input/InputModuleTypes.h"
#include "UObject/Interface.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widgets/Layout/Anchors.h"
#include "PanelWidgetInterface.generated.h"

class UPanelWidget;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPanelWidgetInterface : public UBaseWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IPanelWidgetInterface : public IBaseWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual FName GetWidgetName() const = 0;

	virtual int32 GetWidgetZOrder() const = 0;

	virtual FAnchors GetWidgetAnchors() const = 0;

	virtual bool IsWidgetPenetrable() const = 0;

	virtual bool IsWidgetAutoSize() const = 0;

	virtual FVector2D GetWidgetDrawSize() const = 0;

	virtual FMargin GetWidgetOffsets() const = 0;

	virtual FVector2D GetWidgetAlignment() const = 0;

	virtual EWidgetRefreshType GetWidgetRefreshType() const = 0;

	virtual EInputMode GetWidgetInputMode() const = 0;

	virtual UObject* GetOwnerObject() const = 0;

	virtual UPanelWidget* GetRootPanelWidget() const = 0;
};
