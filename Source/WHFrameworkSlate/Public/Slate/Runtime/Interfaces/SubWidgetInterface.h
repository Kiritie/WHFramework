// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PanelWidgetInterface.h"
#include "Parameter/ParameterTypes.h"

#include "UObject/Interface.h"
#include "SubWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USubWidgetInterface : public UPanelWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API ISubWidgetInterface : public IBaseWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) = 0;

	virtual void OnReset(bool bForce = false) = 0;

	virtual void OnInitialize(const TArray<FParameter>& InParams) = 0;

	virtual void OnDestroy(bool bRecovery) = 0;

public:
	virtual void Init(const TArray<FParameter>* InParams = nullptr) = 0;
	
	virtual void Init(const TArray<FParameter>& InParams) = 0;

	virtual void Reset(bool bForce = false) = 0;

public:
	virtual UUserWidget* GetOwnerWidget(TSubclassOf<UUserWidget> InClass = nullptr) const = 0;

	virtual TArray<FParameter> GetWidgetParams() const = 0;
};
