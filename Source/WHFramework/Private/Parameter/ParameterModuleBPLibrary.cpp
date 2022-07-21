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
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return FParameter();
}

TArray<FParameter> UParameterModuleBPLibrary::GetParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FParameter>();
}

void UParameterModuleBPLibrary::RemoveParameter(FName InName)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->RemoveParameter(InName);
	}
}

void UParameterModuleBPLibrary::ClearAllParameter()
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->ClearAllParameter();
	}
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
	ensureEditor(bEnsured);
	return 0;
}

TArray<int32> UParameterModuleBPLibrary::GetIntegerParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetIntegerParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return 0.f;
}

TArray<float> UParameterModuleBPLibrary::GetFloatParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetFloatParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return TEXT("");
}

TArray<FString> UParameterModuleBPLibrary::GetStringParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetStringParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FString>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetTextParameter(FName InName, const FText InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetTextParameter(InName, InValue);
	}
}

FText UParameterModuleBPLibrary::GetTextParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetTextParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FText::GetEmpty();
}

TArray<FText> UParameterModuleBPLibrary::GetTextParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetTextParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FText>();
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
	ensureEditor(bEnsured);
	return false;
}

TArray<bool> UParameterModuleBPLibrary::GetBooleanParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetBooleanParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return FVector();
}

TArray<FVector> UParameterModuleBPLibrary::GetVectorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetVectorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return FRotator();
}

TArray<FRotator> UParameterModuleBPLibrary::GetRotatorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetRotatorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FRotator>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::SetColorParameter(FName InName, const FColor& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetColorParameter(InName, InValue);
	}
}

FColor UParameterModuleBPLibrary::GetColorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetColorParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FColor();
}

TArray<FColor> UParameterModuleBPLibrary::GetColorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetColorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FColor>();
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
	ensureEditor(bEnsured);
	return nullptr;
}

TArray<UClass*> UParameterModuleBPLibrary::GetClassParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetClassParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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
	ensureEditor(bEnsured);
	return nullptr;
}

TArray<UObject*> UParameterModuleBPLibrary::GetObjectParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetObjectParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
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

FText UParameterModuleBPLibrary::GetTextValue(const FParameter& InParameter)
{
	return InParameter.GetTextValue();
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
