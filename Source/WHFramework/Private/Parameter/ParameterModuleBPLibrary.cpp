// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleBPLibrary.h"

#include "Debug/DebugModuleTypes.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Parameter/ParameterModule.h"

bool UParameterModuleBPLibrary::HasGlobalParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->HasParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return false;
}

void UParameterModuleBPLibrary::AddGlobalParameter(FName InName, FParameter InParameter)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddParameter(InName, InParameter);
	}
}

void UParameterModuleBPLibrary::SetGlobalParameter(FName InName, FParameter InParameter)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetParameter(InName, InParameter);
	}
}

FParameter UParameterModuleBPLibrary::GetGlobalParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FParameter();
}

TArray<FParameter> UParameterModuleBPLibrary::GetGlobalParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FParameter>();
}

void UParameterModuleBPLibrary::RemoveGlobalParameter(FName InName)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->RemoveParameter(InName);
	}
}

void UParameterModuleBPLibrary::RemoveGlobalParameters(FName InName)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->RemoveParameters(InName);
	}
}

void UParameterModuleBPLibrary::ClearAllGlobalParameter()
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->ClearAllParameter();
	}
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalIntegerParameter(FName InName, int32 InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddIntegerParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalIntegerParameter(FName InName, int32 InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetIntegerParameter(InName, InValue);
	}
}

int32 UParameterModuleBPLibrary::GetGlobalIntegerParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetIntegerParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return 0;
}

TArray<int32> UParameterModuleBPLibrary::GetGlobalIntegerParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetIntegerParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<int32>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalFloatParameter(FName InName, float InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddFloatParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalFloatParameter(FName InName, float InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetFloatParameter(InName, InValue);
	}
}

float UParameterModuleBPLibrary::GetGlobalFloatParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetFloatParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return 0.f;
}

TArray<float> UParameterModuleBPLibrary::GetGlobalFloatParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetFloatParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<float>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalStringParameter(FName InName, const FString& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddStringParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalStringParameter(FName InName, const FString& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetStringParameter(InName, InValue);
	}
}

FString UParameterModuleBPLibrary::GetGlobalStringParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetStringParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TEXT("");
}

TArray<FString> UParameterModuleBPLibrary::GetGlobalStringParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetStringParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FString>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalTextParameter(FName InName, const FText InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddTextParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalTextParameter(FName InName, const FText InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetTextParameter(InName, InValue);
	}
}

FText UParameterModuleBPLibrary::GetGlobalTextParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetTextParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FText::GetEmpty();
}

TArray<FText> UParameterModuleBPLibrary::GetGlobalTextParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetTextParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FText>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalBooleanParameter(FName InName, bool InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddBooleanParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalBooleanParameter(FName InName, bool InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetBooleanParameter(InName, InValue);
	}
}

bool UParameterModuleBPLibrary::GetGlobalBooleanParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetBooleanParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return false;
}

TArray<bool> UParameterModuleBPLibrary::GetGlobalBooleanParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetBooleanParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<bool>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalVectorParameter(FName InName, FVector InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddVectorParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalVectorParameter(FName InName, FVector InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetVectorParameter(InName, InValue);
	}
}

FVector UParameterModuleBPLibrary::GetGlobalVectorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetVectorParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FVector();
}

TArray<FVector> UParameterModuleBPLibrary::GetGlobalVectorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetVectorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FVector>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalRotatorParameter(FName InName, FRotator InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddRotatorParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalRotatorParameter(FName InName, FRotator InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetRotatorParameter(InName, InValue);
	}
}

FRotator UParameterModuleBPLibrary::GetGlobalRotatorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetRotatorParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FRotator();
}

TArray<FRotator> UParameterModuleBPLibrary::GetGlobalRotatorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetRotatorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FRotator>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalColorParameter(FName InName, const FColor& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddColorParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalColorParameter(FName InName, const FColor& InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetColorParameter(InName, InValue);
	}
}

FColor UParameterModuleBPLibrary::GetGlobalColorParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetColorParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return FColor();
}

TArray<FColor> UParameterModuleBPLibrary::GetGlobalColorParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetColorParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<FColor>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalClassParameter(FName InName, UClass* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddClassParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalClassParameter(FName InName, UClass* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetClassParameter(InName, InValue);
	}
}

UClass* UParameterModuleBPLibrary::GetGlobalClassParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetClassParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return nullptr;
}

TArray<UClass*> UParameterModuleBPLibrary::GetGlobalClassParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetClassParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<UClass*>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleBPLibrary::AddGlobalObjectParameter(FName InName, UObject* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddObjectParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetGlobalObjectParameter(FName InName, UObject* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetObjectParameter(InName, InValue);
	}
}

UObject* UParameterModuleBPLibrary::GetGlobalObjectParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetObjectParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return nullptr;
}

TArray<UObject*> UParameterModuleBPLibrary::GetGlobalObjectParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetObjectParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<UObject*>();
}

void UParameterModuleBPLibrary::AddPointerParameter(FName InName, void* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->AddPointerParameter(InName, InValue);
	}
}

void UParameterModuleBPLibrary::SetPointerParameter(FName InName, void* InValue)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		ParameterModule->SetPointerParameter(InName, InValue);
	}
}

void* UParameterModuleBPLibrary::GetPointerParameter(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetPointerParameter(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return nullptr;
}

TArray<void*> UParameterModuleBPLibrary::GetPointerParameters(FName InName, bool bEnsured)
{
	if(AParameterModule* ParameterModule = AMainModule::GetModuleByClass<AParameterModule>())
	{
		return ParameterModule->GetPointerParameters(InName, bEnsured);
	}
	ensureEditor(bEnsured);
	return TArray<void*>();
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
