// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleStatics.h"

#include "Parameter/ParameterModule.h"

bool UParameterModuleStatics::HasGlobalParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().HasParameter(InName, bEnsured);
}

void UParameterModuleStatics::SetGlobalParameter(FName InName, FParameter InParameter)
{
	UParameterModule::Get().SetParameter(InName, InParameter);
}

FParameter UParameterModuleStatics::GetGlobalParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetParameter(InName, bEnsured);
}

TArray<FParameter> UParameterModuleStatics::GetGlobalParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetParameters(InName, bEnsured);
}

void UParameterModuleStatics::RemoveGlobalParameter(FName InName)
{
	UParameterModule::Get().RemoveParameter(InName);
}

void UParameterModuleStatics::RemoveGlobalParameters(FName InName)
{
	UParameterModule::Get().RemoveParameters(InName);
}

void UParameterModuleStatics::ClearAllGlobalParameter()
{
	UParameterModule::Get().ClearAllParameter();
}

FParameter UParameterModuleStatics::MakeIntegerParameter(int32 InValue)
{
	return FParameter::MakeInteger(InValue);
}

FParameter UParameterModuleStatics::MakeFloatParameter(float InValue)
{
	return FParameter::MakeFloat(InValue);
}

FParameter UParameterModuleStatics::MakeStringParameter(const FString& InValue)
{
	return FParameter::MakeString(InValue);
}

FParameter UParameterModuleStatics::MakeNameParameter(const FName InValue)
{
	return FParameter::MakeName(InValue);
}

FParameter UParameterModuleStatics::MakeTextParameter(const FText InValue)
{
	return FParameter::MakeText(InValue);
}

FParameter UParameterModuleStatics::MakeBooleanParameter(bool InValue)
{
	return FParameter::MakeBoolean(InValue);
}

FParameter UParameterModuleStatics::MakeVectorParameter(const FVector& InValue)
{
	return FParameter::MakeVector(InValue);
}

FParameter UParameterModuleStatics::MakeRotatorParameter(const FRotator& InValue)
{
	return FParameter::MakeRotator(InValue);
}

FParameter UParameterModuleStatics::MakeColorParameter(FColor InValue)
{
	return FParameter::MakeColor(InValue);
}

FParameter UParameterModuleStatics::MakeKeyParameter(const FKey& InValue)
{
	return FParameter::MakeKey(InValue);
}

FParameter UParameterModuleStatics::MakeTagParameter(const FGameplayTag& InValue)
{
	return FParameter::MakeTag(InValue);
}

FParameter UParameterModuleStatics::MakeTagsParameter(const FGameplayTagContainer& InValue)
{
	return FParameter::MakeTags(InValue);
}

FParameter UParameterModuleStatics::MakeClassParameter(UClass* InValue)
{
	return FParameter::MakeClass(InValue);
}

FParameter UParameterModuleStatics::MakeClassPtrParameter(const TSoftClassPtr<UObject>& InValue)
{
	return FParameter::MakeClassPtr(InValue);
}

FParameter UParameterModuleStatics::MakeObjectParameter(UObject* InValue)
{
	return FParameter::MakeObject(InValue);
}

FParameter UParameterModuleStatics::MakeObjectPtrParameter(const TSoftObjectPtr<UObject>& InValue)
{
	return FParameter::MakeObjectPtr(InValue);
}

FParameter UParameterModuleStatics::MakeDelegateParameter(const FSimpleDynamicDelegate& InValue)
{
	return FParameter::MakeDelegate(InValue);
}
