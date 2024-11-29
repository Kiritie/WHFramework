// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BaseWidgetInterface.h"
#include "WHFrameworkSlateTypes.h"

#include "UObject/Interface.h"
#include "Input/InputTypes.h"
#include "Widgets/Layout/Anchors.h"
#include "PanelWidgetInterface.generated.h"

class ISubWidgetInterface;
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
class WHFRAMEWORKSLATE_API IPanelWidgetInterface : public IBaseWidgetInterface
{
	GENERATED_BODY()

public:
	virtual void OnReset(bool bForce = false) = 0;

public:
	virtual void Reset(bool bForce = false) = 0;

public:
	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>* InParams = nullptr) = 0;

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams) = 0;

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, bool bRecovery = false) = 0;

	virtual void DestroyAllSubWidget(bool bRecovery = false) = 0;

public:
	virtual FName GetWidgetName() const = 0;

	virtual int32 GetWidgetZOrder() const = 0;

	virtual FAnchors GetWidgetAnchors() const = 0;

	virtual bool IsWidgetAutoSize() const = 0;

	virtual FVector2D GetWidgetDrawSize() const = 0;

	virtual FMargin GetWidgetOffsets() const = 0;

	virtual FVector2D GetWidgetAlignment() const = 0;

	virtual EWidgetRefreshType GetWidgetRefreshType() const = 0;

	virtual EInputMode GetWidgetInputMode() const = 0;

	virtual int32 GetSubWidgetNum() const = 0;

	virtual TArray<ISubWidgetInterface*> GetSubWidgets() = 0;

	virtual ISubWidgetInterface* GetSubWidget(int32 InIndex) const = 0;

	virtual int32 FindSubWidget(ISubWidgetInterface* InWidget) const = 0;

	virtual TArray<FParameter> GetWidgetParams() const = 0;

	virtual UObject* GetOwnerObject() const = 0;
};
