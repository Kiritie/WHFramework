
// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModule.h"

#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Module/ParameterSaveGame.h"
		
IMPLEMENTATION_MODULE(AParameterModule)

// ParamSets default values
AParameterModule::AParameterModule()
{
	ModuleName = FName("ParameterModule");
	ModuleSaveGame = UParameterSaveGame::StaticClass();
}

AParameterModule::~AParameterModule()
{
	TERMINATION_MODULE(AParameterModule)
}

#if WITH_EDITOR
void AParameterModule::OnGenerate()
{
	Super::OnGenerate();
}

void AParameterModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AParameterModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AParameterModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		LoadSaveData(USaveGameModuleBPLibrary::GetOrCreateSaveGame(ModuleSaveGame, 0, true)->GetSaveData());
	}
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

void AParameterModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		USaveGameModuleBPLibrary::SaveSaveGame(ModuleSaveGame, 0, true);
	}
}

void AParameterModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Parameters = InSaveData->CastRef<FParameterModuleSaveData>().Parameters;
}

FSaveData* AParameterModule::ToData(bool bRefresh)
{
	static FParameterModuleSaveData SaveData;
	SaveData = FParameterModuleSaveData();
	
	SaveData.Parameters = Parameters;
	return &SaveData;
}

bool AParameterModule::HasParameter(FName InName, bool bEnsured) const
{
	return Parameters.HasParameter(InName, bEnsured);
}

void AParameterModule::AddParameter(FName InName, FParameter InParameter)
{
	Parameters.AddParameter(InName, InParameter);
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

void AParameterModule::RemoveParameter(FName InName)
{
	Parameters.RemoveParameter(InName);
}

void AParameterModule::RemoveParameters(FName InName)
{
	Parameters.RemoveParameters(InName);
}

void AParameterModule::ClearAllParameter()
{
	Parameters.ClearAllParameter();
}

void AParameterModule::AddIntegerParameter(FName InName, int32 InValue)
{
	Parameters.AddIntegerParameter(InName, InValue);
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

void AParameterModule::AddFloatParameter(FName InName, float InValue)
{
	Parameters.AddFloatParameter(InName, InValue);
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

void AParameterModule::AddStringParameter(FName InName, const FString& InValue)
{
	Parameters.AddStringParameter(InName, InValue);
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

void AParameterModule::AddTextParameter(FName InName, const FText InValue)
{
	Parameters.AddTextParameter(InName, InValue);
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

void AParameterModule::AddBooleanParameter(FName InName, bool InValue)
{
	Parameters.AddBooleanParameter(InName, InValue);
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

void AParameterModule::AddVectorParameter(FName InName, FVector InValue)
{
	Parameters.AddVectorParameter(InName, InValue);
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

void AParameterModule::AddRotatorParameter(FName InName, FRotator InValue)
{
	Parameters.AddRotatorParameter(InName, InValue);
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

void AParameterModule::AddColorParameter(FName InName, const FColor& InValue)
{
	Parameters.AddColorParameter(InName, InValue);
}

void AParameterModule::SetColorParameter(FName InName, const FColor& InValue)
{
	Parameters.SetColorParameter(InName, InValue);
}

FColor AParameterModule::GetColorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetColorParameter(InName, bEnsured);
}

TArray<FColor> AParameterModule::GetColorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetColorParameters(InName, bEnsured);
}

void AParameterModule::AddClassParameter(FName InName, UClass* InValue)
{
	Parameters.AddClassParameter(InName, InValue);
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

void AParameterModule::AddObjectParameter(FName InName, UObject* InValue)
{
	Parameters.AddObjectParameter(InName, InValue);
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

void AParameterModule::AddPointerParameter(FName InName, void* InValue)
{
	Parameters.AddPointerParameter(InName, InValue);
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
