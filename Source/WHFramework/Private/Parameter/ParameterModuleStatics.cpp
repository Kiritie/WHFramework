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

FParameter UParameterModuleStatics::MakeIntegerParameter(int32 InValue, const FText InDescription)
{
	return FParameter::MakeInteger(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeFloatParameter(float InValue, const FText InDescription)
{
	return FParameter::MakeFloat(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeByteParameter(uint8 InValue, const FText InDescription)
{
	return FParameter::MakeByte(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeStringParameter(const FString& InValue, const FText InDescription)
{
	return FParameter::MakeString(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeNameParameter(const FName InValue, const FText InDescription)
{
	return FParameter::MakeName(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeTextParameter(const FText InValue, const FText InDescription)
{
	return FParameter::MakeText(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeBooleanParameter(bool InValue, const FText InDescription)
{
	return FParameter::MakeBoolean(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeVectorParameter(const FVector& InValue, const FText InDescription)
{
	return FParameter::MakeVector(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeRotatorParameter(const FRotator& InValue, const FText InDescription)
{
	return FParameter::MakeRotator(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeColorParameter(FColor InValue, const FText InDescription)
{
	return FParameter::MakeColor(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeKeyParameter(const FKey& InValue, const FText InDescription)
{
	return FParameter::MakeKey(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeTagParameter(const FGameplayTag& InValue, const FText InDescription)
{
	return FParameter::MakeTag(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeTagsParameter(const FGameplayTagContainer& InValue, const FText InDescription)
{
	return FParameter::MakeTags(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeBrushParameter(const FSlateBrush& InValue, const FText InDescription)
{
	return FParameter::MakeBrush(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeAssetIDParameter(const FPrimaryAssetId& InValue, const FText InDescription)
{
	return FParameter::MakeAssetID(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeClassParameter(UClass* InValue, const FText InDescription)
{
	return FParameter::MakeClass(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeClassPtrParameter(const TSoftClassPtr<UObject> InValue, const FText InDescription)
{
	return FParameter::MakeClassPtr(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeObjectParameter(UObject* InValue, const FText InDescription)
{
	return FParameter::MakeObject(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeObjectInstParameter(UObject* InValue, const FText InDescription)
{
	return FParameter::MakeObjectInst(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeObjectPtrParameter(const TSoftObjectPtr<UObject> InValue, const FText InDescription)
{
	return FParameter::MakeObjectPtr(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeActorPtrParameter(const TSoftObjectPtr<AActor> InValue, const FText InDescription)
{
	return FParameter::MakeObjectPtr(InValue, InDescription);
}

FParameter UParameterModuleStatics::MakeDelegateParameter(const FSimpleDynamicDelegate& InValue, const FText InDescription)
{
	return FParameter::MakeDelegate(InValue, InDescription);
}
