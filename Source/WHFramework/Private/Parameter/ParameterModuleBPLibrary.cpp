// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleBPLibrary.h"

#include "Debug/DebugModuleTypes.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Parameter/ParameterModule.h"

bool UParameterModuleBPLibrary::HasParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->HasParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return false;
}

void UParameterModuleBPLibrary::SetParameter(FName InName, FParameter InParameter)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetParameter(InName, InParameter);
	}
}

FParameter UParameterModuleBPLibrary::GetParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FParameter();
}

TArray<FParameter> UParameterModuleBPLibrary::GetParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FParameter>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetIntegerParameter(FName InName, int32 InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetIntegerParameter(InName, InValue);
	}
}

int32 UParameterModuleBPLibrary::GetIntegerParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetIntegerParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return 0;
}

TArray<int32> UParameterModuleBPLibrary::GetIntegerParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetIntegerParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<int32>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetFloatParameter(FName InName, float InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetFloatParameter(InName, InValue);
	}
}

float UParameterModuleBPLibrary::GetFloatParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetFloatParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return 0.f;
}

TArray<float> UParameterModuleBPLibrary::GetFloatParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetFloatParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<float>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetStringParameter(FName InName, const FString& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetStringParameter(InName, InValue);
	}
}

FString UParameterModuleBPLibrary::GetStringParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetStringParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TEXT("");
}

TArray<FString> UParameterModuleBPLibrary::GetStringParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetStringParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FString>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetBooleanParameter(FName InName, bool InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetBooleanParameter(InName, InValue);
	}
}

bool UParameterModuleBPLibrary::GetBooleanParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetBooleanParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return false;
}

TArray<bool> UParameterModuleBPLibrary::GetBooleanParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetBooleanParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<bool>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetVectorParameter(FName InName, FVector InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetVectorParameter(InName, InValue);
	}
}

FVector UParameterModuleBPLibrary::GetVectorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetVectorParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FVector();
}

TArray<FVector> UParameterModuleBPLibrary::GetVectorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetVectorParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FVector>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetRotatorParameter(FName InName, FRotator InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetRotatorParameter(InName, InValue);
	}
}

FRotator UParameterModuleBPLibrary::GetRotatorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetRotatorParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return FRotator();
}

TArray<FRotator> UParameterModuleBPLibrary::GetRotatorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetRotatorParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<FRotator>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetClassParameter(FName InName, UClass* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetClassParameter(InName, InValue);
	}
}

UClass* UParameterModuleBPLibrary::GetClassParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetClassParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return nullptr;
}

TArray<UClass*> UParameterModuleBPLibrary::GetClassParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetClassParameters(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return TArray<UClass*>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetObjectParameter(FName InName, UObject* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetObjectParameter(InName, InValue);
	}
}

UObject* UParameterModuleBPLibrary::GetObjectParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetObjectParameter(InName, bEnsured);
	}
	if(bEnsured) ensureEditor(false);
	return nullptr;
}

TArray<UObject*> UParameterModuleBPLibrary::GetObjectParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
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
