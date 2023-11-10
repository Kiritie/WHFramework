// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleStatics.h"

#include "Debug/DebugModuleTypes.h"
#include "Parameter/ParameterModule.h"

bool UParameterModuleStatics::HasGlobalParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().HasParameter(InName, bEnsured);
}

void UParameterModuleStatics::AddGlobalParameter(FName InName, FParameter InParameter)
{
		UParameterModule::Get().AddParameter(InName, InParameter);
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

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalIntegerParameter(FName InName, int32 InValue)
{
	UParameterModule::Get().AddIntegerParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalIntegerParameter(FName InName, int32 InValue)
{
	UParameterModule::Get().SetIntegerParameter(InName, InValue);
}

int32 UParameterModuleStatics::GetGlobalIntegerParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetIntegerParameter(InName, bEnsured);
}

TArray<int32> UParameterModuleStatics::GetGlobalIntegerParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetIntegerParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalFloatParameter(FName InName, float InValue)
{
	UParameterModule::Get().AddFloatParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalFloatParameter(FName InName, float InValue)
{
	UParameterModule::Get().SetFloatParameter(InName, InValue);
}

float UParameterModuleStatics::GetGlobalFloatParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetFloatParameter(InName, bEnsured);
}

TArray<float> UParameterModuleStatics::GetGlobalFloatParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetFloatParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalStringParameter(FName InName, const FString& InValue)
{
	UParameterModule::Get().AddStringParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalStringParameter(FName InName, const FString& InValue)
{
	UParameterModule::Get().SetStringParameter(InName, InValue);
}

FString UParameterModuleStatics::GetGlobalStringParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetStringParameter(InName, bEnsured);
}

TArray<FString> UParameterModuleStatics::GetGlobalStringParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetStringParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalTextParameter(FName InName, const FText InValue)
{
	UParameterModule::Get().AddTextParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalTextParameter(FName InName, const FText InValue)
{
	UParameterModule::Get().SetTextParameter(InName, InValue);
}

FText UParameterModuleStatics::GetGlobalTextParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetTextParameter(InName, bEnsured);
}

TArray<FText> UParameterModuleStatics::GetGlobalTextParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetTextParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalBooleanParameter(FName InName, bool InValue)
{
	UParameterModule::Get().AddBooleanParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalBooleanParameter(FName InName, bool InValue)
{
	UParameterModule::Get().SetBooleanParameter(InName, InValue);
}

bool UParameterModuleStatics::GetGlobalBooleanParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetBooleanParameter(InName, bEnsured);
}

TArray<bool> UParameterModuleStatics::GetGlobalBooleanParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetBooleanParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalVectorParameter(FName InName, FVector InValue)
{
	UParameterModule::Get().AddVectorParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalVectorParameter(FName InName, FVector InValue)
{
	UParameterModule::Get().SetVectorParameter(InName, InValue);
}

FVector UParameterModuleStatics::GetGlobalVectorParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetVectorParameter(InName, bEnsured);
}

TArray<FVector> UParameterModuleStatics::GetGlobalVectorParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetVectorParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalRotatorParameter(FName InName, FRotator InValue)
{
	UParameterModule::Get().AddRotatorParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalRotatorParameter(FName InName, FRotator InValue)
{
	UParameterModule::Get().SetRotatorParameter(InName, InValue);
}

FRotator UParameterModuleStatics::GetGlobalRotatorParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetRotatorParameter(InName, bEnsured);
}

TArray<FRotator> UParameterModuleStatics::GetGlobalRotatorParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetRotatorParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalColorParameter(FName InName, const FColor& InValue)
{
	UParameterModule::Get().AddColorParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalColorParameter(FName InName, const FColor& InValue)
{
	UParameterModule::Get().SetColorParameter(InName, InValue);
}

FColor UParameterModuleStatics::GetGlobalColorParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetColorParameter(InName, bEnsured);
}

TArray<FColor> UParameterModuleStatics::GetGlobalColorParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetColorParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalClassParameter(FName InName, UClass* InValue)
{
	UParameterModule::Get().AddClassParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalClassParameter(FName InName, UClass* InValue)
{
	UParameterModule::Get().SetClassParameter(InName, InValue);
}

UClass* UParameterModuleStatics::GetGlobalClassParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetClassParameter(InName, bEnsured);
}

TArray<UClass*> UParameterModuleStatics::GetGlobalClassParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetClassParameters(InName, bEnsured);
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalObjectParameter(FName InName, UObject* InValue)
{
	UParameterModule::Get().AddObjectParameter(InName, InValue);
}

void UParameterModuleStatics::SetGlobalObjectParameter(FName InName, UObject* InValue)
{
	UParameterModule::Get().SetObjectParameter(InName, InValue);
}

UObject* UParameterModuleStatics::GetGlobalObjectParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetObjectParameter(InName, bEnsured);
}

TArray<UObject*> UParameterModuleStatics::GetGlobalObjectParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetObjectParameters(InName, bEnsured);
}

void UParameterModuleStatics::AddPointerParameter(FName InName, void* InValue)
{
	UParameterModule::Get().AddPointerParameter(InName, InValue);
}

void UParameterModuleStatics::SetPointerParameter(FName InName, void* InValue)
{
	UParameterModule::Get().SetPointerParameter(InName, InValue);
}

void* UParameterModuleStatics::GetPointerParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetPointerParameter(InName, bEnsured);
}

TArray<void*> UParameterModuleStatics::GetPointerParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetPointerParameters(InName, bEnsured);
}

FParameter UParameterModuleStatics::MakeIntegerParameter(int32 InValue)
{
	return FParameter::MakeInteger(InValue);
}

FParameter UParameterModuleStatics::MakeFloatParameter(float InValue)
{
	return FParameter::MakeFloat(InValue);
}

FParameter UParameterModuleStatics::MakeStringParameter(FString InValue)
{
	return FParameter::MakeString(InValue);
}

FParameter UParameterModuleStatics::MakeTextParameter(FText InValue)
{
	return FParameter::MakeText(InValue);
}

FParameter UParameterModuleStatics::MakeBooleanParameter(bool InValue)
{
	return FParameter::MakeBoolean(InValue);
}

FParameter UParameterModuleStatics::MakeVectorParameter(FVector InValue)
{
	return FParameter::MakeVector(InValue);
}

FParameter UParameterModuleStatics::MakeRotatorParameter(FRotator InValue)
{
	return FParameter::MakeRotator(InValue);
}

FParameter UParameterModuleStatics::MakeColorParameter(FColor InValue)
{
	return FParameter::MakeColor(InValue);
}

FParameter UParameterModuleStatics::MakeClassParameter(UClass* InValue)
{
	return FParameter::MakeClass(InValue);
}

FParameter UParameterModuleStatics::MakeObjectParameter(UObject* InValue)
{
	return FParameter::MakeObject(InValue);
}
