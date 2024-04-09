// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ParameterTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

/**
 * 
 */
class WHFRAMEWORKCORE_API FParameterManager : public FManagerBase
{
	GENERATED_MANAGER(FParameterManager)

public:	
	// ParamSets default values for this actor's properties
	FParameterManager();

	virtual ~FParameterManager() override;
	
	static const FUniqueType Type;

public:
	virtual bool HasParameter(FName InName, bool bEnsured = true) const { return false; }

	virtual void SetParameter(FName InName, FParameter InParameter) { }

	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const { return FParameter(); }

	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const { return TArray<FParameter>(); }

	virtual void RemoveParameter(FName InName) { }

	virtual void RemoveParameters(FName InName) { }

	virtual TArray<FParameterSet> GetAllParameter() { return TArray<FParameterSet>(); }

	virtual void ClearAllParameter() { }
};
