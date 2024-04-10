// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ParameterManagerInterface.h"
#include "ParameterTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

/**
 * 
 */
class WHFRAMEWORKCORE_API FParameterManager : public FManagerBase, public IParameterManagerInterface
{
	GENERATED_MANAGER(FParameterManager)

public:	
	// ParamSets default values for this actor's properties
	FParameterManager();

	virtual ~FParameterManager() override;
	
	static const FUniqueType Type;

public:
	virtual bool HasParameter(FName InName, bool bEnsured = true) const override { return false; }

	virtual void SetParameter(FName InName, FParameter InParameter) override { }

	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const override { return FParameter(); }

	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const override { return TArray<FParameter>(); }

	virtual void RemoveParameter(FName InName) override { }

	virtual void RemoveParameters(FName InName) override { }

	virtual TArray<FParameterSet> GetAllParameter() override { return TArray<FParameterSet>(); }

	virtual void ClearAllParameter() override { }
};
