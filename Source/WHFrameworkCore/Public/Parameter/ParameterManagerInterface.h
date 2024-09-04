// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ParameterTypes.h"
#include "UObject/Interface.h"
#include "ParameterManagerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UParameterManagerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKCORE_API IParameterManagerInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool HasParameter(FName InName, bool bEnsured = true) const = 0;

	virtual void SetParameter(FName InName, FParameter InParameter) = 0;

	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const = 0;

	virtual void RemoveParameter(FName InName) = 0;

	virtual void RemoveParameters(FName InName) = 0;

	virtual TArray<FParameterSet> GetAllParameter() = 0;

	virtual void ClearAllParameter() = 0;
};
