// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "UObject/Interface.h"
#include "ObjectPoolInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UObjectPoolInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IObjectPoolInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetLimit() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, meta = (AutoCreateRefTerm = "InParams"))
	void OnSpawn(const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnDespawn(bool bRecovery);
};
