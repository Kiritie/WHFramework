// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Parameter/ParameterModule.h"

AParameterModule* UParameterModuleBPLibrary::ParameterModuleInst = nullptr;

AParameterModule* UParameterModuleBPLibrary::GetParameterModule(UObject* InWorldContext)
{
	if (!ParameterModuleInst || !ParameterModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			ParameterModuleInst = MainModule->GetModuleByClass<AParameterModule>();
		}
	}
	return ParameterModuleInst;
}

bool UParameterModuleBPLibrary::HasParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->HasParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return false;
}

void UParameterModuleBPLibrary::SetParameter(UObject* InWorldContext, FName InName, FParameter InParameter)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetParameter(InName, InParameter);
	}
}

FParameter UParameterModuleBPLibrary::GetParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FParameter();
}

TArray<FParameter> UParameterModuleBPLibrary::GetParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FParameter>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetIntegerParameter(UObject* InWorldContext, FName InName, int32 InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetIntegerParameter(InName, InValue);
	}
}

int32 UParameterModuleBPLibrary::GetIntegerParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetIntegerParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return 0;
}

TArray<int32> UParameterModuleBPLibrary::GetIntegerParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetIntegerParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<int32>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetFloatParameter(UObject* InWorldContext, FName InName, float InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetFloatParameter(InName, InValue);
	}
}

float UParameterModuleBPLibrary::GetFloatParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetFloatParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return 0.f;
}

TArray<float> UParameterModuleBPLibrary::GetFloatParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetFloatParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<float>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetStringParameter(UObject* InWorldContext, FName InName, const FString& InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetStringParameter(InName, InValue);
	}
}

FString UParameterModuleBPLibrary::GetStringParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetStringParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TEXT("");
}

TArray<FString> UParameterModuleBPLibrary::GetStringParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetStringParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FString>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetBooleanParameter(UObject* InWorldContext, FName InName, bool InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetBooleanParameter(InName, InValue);
	}
}

bool UParameterModuleBPLibrary::GetBooleanParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetBooleanParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return false;
}

TArray<bool> UParameterModuleBPLibrary::GetBooleanParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetBooleanParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<bool>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetVectorParameter(UObject* InWorldContext, FName InName, FVector InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetVectorParameter(InName, InValue);
	}
}

FVector UParameterModuleBPLibrary::GetVectorParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetVectorParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FVector();
}

TArray<FVector> UParameterModuleBPLibrary::GetVectorParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetVectorParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FVector>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetRotatorParameter(UObject* InWorldContext, FName InName, FRotator InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetRotatorParameter(InName, InValue);
	}
}

FRotator UParameterModuleBPLibrary::GetRotatorParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetRotatorParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FRotator();
}

TArray<FRotator> UParameterModuleBPLibrary::GetRotatorParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetRotatorParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FRotator>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetClassParameter(UObject* InWorldContext, FName InName, UClass* InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetClassParameter(InName, InValue);
	}
}

UClass* UParameterModuleBPLibrary::GetClassParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetClassParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return nullptr;
}

TArray<UClass*> UParameterModuleBPLibrary::GetClassParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetClassParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<UClass*>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetObjectParameter(UObject* InWorldContext, FName InName, UObject* InValue)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		ParameterModule->SetObjectParameter(InName, InValue);
	}
}

UObject* UParameterModuleBPLibrary::GetObjectParameter(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetObjectParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return nullptr;
}

TArray<UObject*> UParameterModuleBPLibrary::GetObjectParameters(UObject* InWorldContext, FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = GetParameterModule(InWorldContext))
	{
		return ParameterModule->GetObjectParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<UObject*>();
}

FParameter UParameterModuleBPLibrary::MakeIntegerParameter(int32 InValue)
{
	return FParameter::MakeInteger(InValue);
}

FParameter UParameterModuleBPLibrary::MakeFloatParameter(float InValue)
{
	return FParameter::MakeFloat(InValue);
}

FParameter UParameterModuleBPLibrary::MakeStringParameter(FString InValue)
{
	return FParameter::MakeString(InValue);
}

FParameter UParameterModuleBPLibrary::MakeBooleanParameter(bool InValue)
{
	return FParameter::MakeBoolean(InValue);
}

FParameter UParameterModuleBPLibrary::MakeVectorParameter(FVector InValue)
{
	return FParameter::MakeVector(InValue);
}

FParameter UParameterModuleBPLibrary::MakeRotatorParameter(FRotator InValue)
{
	return FParameter::MakeRotator(InValue);
}

FParameter UParameterModuleBPLibrary::MakeClassParameter(UClass* InValue)
{
	return FParameter::MakeClass(InValue);
}

FParameter UParameterModuleBPLibrary::MakeObjectParameter(UObject* InValue)
{
	return FParameter::MakeObject(InValue);
}

int32 UParameterModuleBPLibrary::GetIntegerValue(const FParameter& InParameter)
{
	return InParameter.GetIntegerValue();
}

float UParameterModuleBPLibrary::GetFloatValue(const FParameter& InParameter)
{
	return InParameter.GetFloatValue();
}

FString UParameterModuleBPLibrary::GetStringValue(const FParameter& InParameter)
{
	return InParameter.GetStringValue();
}

bool UParameterModuleBPLibrary::GetBooleanValue(const FParameter& InParameter)
{
	return InParameter.GetBooleanValue();
}

FVector UParameterModuleBPLibrary::GetVectorValue(const FParameter& InParameter)
{
	return InParameter.GetVectorValue();
}

FRotator UParameterModuleBPLibrary::GetRotatorValue(const FParameter& InParameter)
{
	return InParameter.GetRotatorValue();
}

UClass* UParameterModuleBPLibrary::GetClassValue(const FParameter& InParameter)
{
	return InParameter.GetClassValue();
}

UObject* UParameterModuleBPLibrary::GetObjectValue(const FParameter& InParameter)
{
	return InParameter.GetObjectValue();
}
