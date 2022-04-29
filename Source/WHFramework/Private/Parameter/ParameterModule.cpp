// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModule.h"

#include "Net/UnrealNetwork.h"

// ParamSets default values
AParameterModule::AParameterModule()
{
	ModuleName = FName("ParameterModule");
}

#if WITH_EDITOR
void AParameterModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AParameterModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AParameterModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AParameterModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AParameterModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AParameterModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AParameterModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool AParameterModule::HasParameter(FName InName, bool bEnsured) const
{
	return Parameters.HasParameter(InName, bEnsured);
}

void AParameterModule::SetParameter(FName InName, FParameter InParameter)
{
	Parameters.SetParameter(InName, InParameter);
}

FParameter AParameterModule::GetParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetParameter(InName, bEnsured);
}

TArray<FParameter> AParameterModule::GetParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetParameters(InName, bEnsured);
}

void AParameterModule::SetIntegerParameter(FName InName, int32 InValue)
{
	Parameters.SetIntegerParameter(InName, InValue);
}

int32 AParameterModule::GetIntegerParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetIntegerParameter(InName, bEnsured);
}

TArray<int32> AParameterModule::GetIntegerParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetIntegerParameters(InName, bEnsured);
}

void AParameterModule::SetFloatParameter(FName InName, float InValue)
{
	Parameters.SetFloatParameter(InName, InValue);
}

float AParameterModule::GetFloatParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetFloatParameter(InName, bEnsured);
}

TArray<float> AParameterModule::GetFloatParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetFloatParameters(InName, bEnsured);
}

void AParameterModule::SetStringParameter(FName InName, const FString& InValue)
{
	Parameters.SetStringParameter(InName, InValue);
}

FString AParameterModule::GetStringParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetStringParameter(InName, bEnsured);
}

TArray<FString> AParameterModule::GetStringParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetStringParameters(InName, bEnsured);
}

void AParameterModule::SetTextParameter(FName InName, const FText InValue)
{
	Parameters.SetTextParameter(InName, InValue);
}

FText AParameterModule::GetTextParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetTextParameter(InName, bEnsured);
}

TArray<FText> AParameterModule::GetTextParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetTextParameters(InName, bEnsured);
}

void AParameterModule::SetBooleanParameter(FName InName, bool InValue)
{
	Parameters.SetBooleanParameter(InName, InValue);
}

bool AParameterModule::GetBooleanParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetBooleanParameter(InName, bEnsured);
}

TArray<bool> AParameterModule::GetBooleanParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetBooleanParameters(InName, bEnsured);
}

void AParameterModule::SetVectorParameter(FName InName, FVector InValue)
{
	Parameters.SetVectorParameter(InName, InValue);
}

FVector AParameterModule::GetVectorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetVectorParameter(InName, bEnsured);
}

TArray<FVector> AParameterModule::GetVectorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetVectorParameters(InName, bEnsured);
}

void AParameterModule::SetRotatorParameter(FName InName, FRotator InValue)
{
	Parameters.SetRotatorParameter(InName, InValue);
}

FRotator AParameterModule::GetRotatorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetRotatorParameter(InName, bEnsured);
}

TArray<FRotator> AParameterModule::GetRotatorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetRotatorParameters(InName, bEnsured);
}

void AParameterModule::SetClassParameter(FName InName, UClass* InValue)
{
	Parameters.SetClassParameter(InName, InValue);
}

UClass* AParameterModule::GetClassParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetClassParameter(InName, bEnsured);
}

TArray<UClass*> AParameterModule::GetClassParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetClassParameters(InName, bEnsured);
}

void AParameterModule::SetObjectParameter(FName InName, UObject* InValue)
{
	Parameters.SetObjectParameter(InName, InValue);
}

UObject* AParameterModule::GetObjectParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetObjectParameter(InName, bEnsured);
}

TArray<UObject*> AParameterModule::GetObjectParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetObjectParameters(InName, bEnsured);
}

void AParameterModule::SetPointerParameter(FName InName, void* InValue)
{
	Parameters.SetPointerParameter(InName, InValue);
}

void* AParameterModule::GetPointerParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetPointerParameter(InName, bEnsured);
}

TArray<void*> AParameterModule::GetPointerParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetPointerParameters(InName, bEnsured);
}

void AParameterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AParameterModule, Parameters);
}
