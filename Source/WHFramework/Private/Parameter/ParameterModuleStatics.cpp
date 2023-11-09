// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleStatics.h"

#include "Debug/DebugModuleTypes.h"
#include "Parameter/ParameterModule.h"

bool UParameterModuleStatics::HasGlobalParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->HasParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return false;
}

void UParameterModuleStatics::AddGlobalParameter(FName InName, FParameter InParameter)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddParameter(InName, InParameter);
	}
}

void UParameterModuleStatics::SetGlobalParameter(FName InName, FParameter InParameter)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetParameter(InName, InParameter);
	}
}

FParameter UParameterModuleStatics::GetGlobalParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return FParameter();
}

TArray<FParameter> UParameterModuleStatics::GetGlobalParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FParameter>();
}

void UParameterModuleStatics::RemoveGlobalParameter(FName InName)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->RemoveParameter(InName);
	}
}

void UParameterModuleStatics::RemoveGlobalParameters(FName InName)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->RemoveParameters(InName);
	}
}

void UParameterModuleStatics::ClearAllGlobalParameter()
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->ClearAllParameter();
	}
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalIntegerParameter(FName InName, int32 InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddIntegerParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalIntegerParameter(FName InName, int32 InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetIntegerParameter(InName, InValue);
	}
}

int32 UParameterModuleStatics::GetGlobalIntegerParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetIntegerParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return 0;
}

TArray<int32> UParameterModuleStatics::GetGlobalIntegerParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetIntegerParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<int32>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalFloatParameter(FName InName, float InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddFloatParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalFloatParameter(FName InName, float InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetFloatParameter(InName, InValue);
	}
}

float UParameterModuleStatics::GetGlobalFloatParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetFloatParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return 0.f;
}

TArray<float> UParameterModuleStatics::GetGlobalFloatParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetFloatParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<float>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalStringParameter(FName InName, const FString& InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddStringParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalStringParameter(FName InName, const FString& InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetStringParameter(InName, InValue);
	}
}

FString UParameterModuleStatics::GetGlobalStringParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetStringParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TEXT("");
}

TArray<FString> UParameterModuleStatics::GetGlobalStringParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetStringParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FString>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalTextParameter(FName InName, const FText InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddTextParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalTextParameter(FName InName, const FText InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetTextParameter(InName, InValue);
	}
}

FText UParameterModuleStatics::GetGlobalTextParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetTextParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return FText::GetEmpty();
}

TArray<FText> UParameterModuleStatics::GetGlobalTextParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetTextParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FText>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalBooleanParameter(FName InName, bool InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddBooleanParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalBooleanParameter(FName InName, bool InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetBooleanParameter(InName, InValue);
	}
}

bool UParameterModuleStatics::GetGlobalBooleanParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetBooleanParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return false;
}

TArray<bool> UParameterModuleStatics::GetGlobalBooleanParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetBooleanParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<bool>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalVectorParameter(FName InName, FVector InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddVectorParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalVectorParameter(FName InName, FVector InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetVectorParameter(InName, InValue);
	}
}

FVector UParameterModuleStatics::GetGlobalVectorParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetVectorParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return FVector();
}

TArray<FVector> UParameterModuleStatics::GetGlobalVectorParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetVectorParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FVector>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalRotatorParameter(FName InName, FRotator InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddRotatorParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalRotatorParameter(FName InName, FRotator InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetRotatorParameter(InName, InValue);
	}
}

FRotator UParameterModuleStatics::GetGlobalRotatorParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetRotatorParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return FRotator();
}

TArray<FRotator> UParameterModuleStatics::GetGlobalRotatorParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetRotatorParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FRotator>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalColorParameter(FName InName, const FColor& InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddColorParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalColorParameter(FName InName, const FColor& InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetColorParameter(InName, InValue);
	}
}

FColor UParameterModuleStatics::GetGlobalColorParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetColorParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return FColor();
}

TArray<FColor> UParameterModuleStatics::GetGlobalColorParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetColorParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<FColor>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalClassParameter(FName InName, UClass* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddClassParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalClassParameter(FName InName, UClass* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetClassParameter(InName, InValue);
	}
}

UClass* UParameterModuleStatics::GetGlobalClassParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetClassParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return nullptr;
}

TArray<UClass*> UParameterModuleStatics::GetGlobalClassParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetClassParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<UClass*>();
}

//////////////////////////////////////////////////////////////////////////
void UParameterModuleStatics::AddGlobalObjectParameter(FName InName, UObject* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddObjectParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetGlobalObjectParameter(FName InName, UObject* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetObjectParameter(InName, InValue);
	}
}

UObject* UParameterModuleStatics::GetGlobalObjectParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetObjectParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return nullptr;
}

TArray<UObject*> UParameterModuleStatics::GetGlobalObjectParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetObjectParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<UObject*>();
}

void UParameterModuleStatics::AddPointerParameter(FName InName, void* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->AddPointerParameter(InName, InValue);
	}
}

void UParameterModuleStatics::SetPointerParameter(FName InName, void* InValue)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		ParameterModule->SetPointerParameter(InName, InValue);
	}
}

void* UParameterModuleStatics::GetPointerParameter(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetPointerParameter(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return nullptr;
}

TArray<void*> UParameterModuleStatics::GetPointerParameters(FName InName, bool bEnsured)
{
	if(UParameterModule* ParameterModule = UParameterModule::Get())
	{
		return ParameterModule->GetPointerParameters(InName, bEnsured);
	}
	ensureEditor(!bEnsured);
	return TArray<void*>();
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
