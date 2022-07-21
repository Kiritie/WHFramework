// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ParameterModuleTypes.h"
#include "UObject/Interface.h"
#include "ParameterManager.generated.h"

class ATargetPoint;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UParameterManager : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IParameterManager
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool HasParameter(FName InName, bool bEnsured = true) const = 0;

	virtual void SetParameter(FName InName, FParameter InParameter) = 0;

	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const = 0;

	virtual void RemoveParameter(FName InName) = 0;

	virtual void ClearAllParameter() = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetIntegerParameter(FName InName, int32 InValue) = 0;

	virtual int32 GetIntegerParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<int32> GetIntegerParameters(FName InName, bool bEnsured = true) const = 0;
	
	//////////////////////////////////////////////////////////////////////////
	virtual void SetFloatParameter(FName InName, float InValue) = 0;

	virtual float GetFloatParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<float> GetFloatParameters(FName InName, bool bEnsured = true) const = 0;
	
	//////////////////////////////////////////////////////////////////////////
	virtual void SetStringParameter(FName InName, const FString& InValue) = 0;

	virtual FString GetStringParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FString> GetStringParameters(FName InName, bool bEnsured = true) const = 0;
		
	//////////////////////////////////////////////////////////////////////////
	virtual void SetTextParameter(FName InName, const FText InValue) = 0;

	virtual FText GetTextParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FText> GetTextParameters(FName InName, bool bEnsured = true) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetBooleanParameter(FName InName, bool InValue) = 0;

	virtual bool GetBooleanParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<bool> GetBooleanParameters(FName InName, bool bEnsured = true) const = 0;
	
	//////////////////////////////////////////////////////////////////////////
	virtual void SetVectorParameter(FName InName, FVector InValue) = 0;

	virtual FVector GetVectorParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FVector> GetVectorParameters(FName InName, bool bEnsured = true) const = 0;
	
	//////////////////////////////////////////////////////////////////////////
	virtual void SetRotatorParameter(FName InName, FRotator InValue) = 0;

	virtual FRotator GetRotatorParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FRotator> GetRotatorParameters(FName InName, bool bEnsured = true) const = 0;
		
	//////////////////////////////////////////////////////////////////////////
	virtual void SetColorParameter(FName InName, const FColor& InValue) = 0;

	virtual FColor GetColorParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<FColor> GetColorParameters(FName InName, bool bEnsured = true) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetClassParameter(FName InName, UClass* InValue) = 0;

	virtual UClass* GetClassParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true) const = 0;
	
	//////////////////////////////////////////////////////////////////////////
	virtual void SetObjectParameter(FName InName, UObject* InValue) = 0;

	virtual UObject* GetObjectParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetPointerParameter(FName InName, void* InValue) = 0;

	virtual void* GetPointerParameter(FName InName, bool bEnsured = true) const = 0;

	virtual TArray<void*> GetPointerParameters(FName InName, bool bEnsured = true) const = 0;
};
