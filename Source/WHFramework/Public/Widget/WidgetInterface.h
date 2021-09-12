// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WidgetModuleTypes.h"
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
	virtual void OnInitialize(AActor* InOwner = nullptr) = 0;

	virtual void OnOpen() = 0;
	
	virtual void OnClose() = 0;
	
	virtual void OnToggle() = 0;

	virtual void OnRefresh() = 0;

	virtual void OnDestroy() = 0;

protected:
	virtual void OpenSelf() = 0;

	virtual void CloseSelf() = 0;

public:
	virtual EWidgetType GetWidgetType() const = 0;

	virtual EInputMode GetInputMode() const = 0;

	virtual AActor* GetOwnerActor() const = 0;
};
