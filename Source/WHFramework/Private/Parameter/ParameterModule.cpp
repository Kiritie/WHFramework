
// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModule.h"

#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/ParameterSaveGame.h"
		
IMPLEMENTATION_MODULE(UParameterModule)

// ParamSets default values
UParameterModule::UParameterModule()
{
	ModuleName = FName("ParameterModule");
	ModuleDisplayName = FText::FromString(TEXT("Parameter Module"));
	ModuleSaveGame = UParameterSaveGame::StaticClass();
}

UParameterModule::~UParameterModule()
{
	TERMINATION_MODULE(UParameterModule)
}

#if WITH_EDITOR
void UParameterModule::OnGenerate()
{
	Super::OnGenerate();
}

void UParameterModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UParameterModule)
}
#endif

void UParameterModule::OnInitialize()
{
	Super::OnInitialize();
}

void UParameterModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}
}

void UParameterModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UParameterModule::OnPause()
{
	Super::OnPause();
}

void UParameterModule::OnUnPause()
{
	Super::OnUnPause();
}

void UParameterModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void UParameterModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FParameterModuleSaveData>();

	Parameters = SaveData.Parameters;
}

FSaveData* UParameterModule::ToData()
{
	static FParameterModuleSaveData SaveData;
	SaveData = FParameterModuleSaveData();
	
	SaveData.Parameters = Parameters;
	return &SaveData;
}

bool UParameterModule::HasParameter(FName InName, bool bEnsured) const
{
	return Parameters.HasParameter(InName, bEnsured);
}

void UParameterModule::AddParameter(FName InName, FParameter InParameter)
{
	Parameters.AddParameter(InName, InParameter);
}

void UParameterModule::SetParameter(FName InName, FParameter InParameter)
{
	Parameters.SetParameter(InName, InParameter);
}

FParameter UParameterModule::GetParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetParameter(InName, bEnsured);
}

TArray<FParameter> UParameterModule::GetParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetParameters(InName, bEnsured);
}

void UParameterModule::RemoveParameter(FName InName)
{
	Parameters.RemoveParameter(InName);
}

void UParameterModule::RemoveParameters(FName InName)
{
	Parameters.RemoveParameters(InName);
}

void UParameterModule::ClearAllParameter()
{
	Parameters.ClearAllParameter();
}

void UParameterModule::AddIntegerParameter(FName InName, int32 InValue)
{
	Parameters.AddIntegerParameter(InName, InValue);
}

void UParameterModule::SetIntegerParameter(FName InName, int32 InValue)
{
	Parameters.SetIntegerParameter(InName, InValue);
}

int32 UParameterModule::GetIntegerParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetIntegerParameter(InName, bEnsured);
}

TArray<int32> UParameterModule::GetIntegerParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetIntegerParameters(InName, bEnsured);
}

void UParameterModule::AddFloatParameter(FName InName, float InValue)
{
	Parameters.AddFloatParameter(InName, InValue);
}

void UParameterModule::SetFloatParameter(FName InName, float InValue)
{
	Parameters.SetFloatParameter(InName, InValue);
}

float UParameterModule::GetFloatParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetFloatParameter(InName, bEnsured);
}

TArray<float> UParameterModule::GetFloatParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetFloatParameters(InName, bEnsured);
}

void UParameterModule::AddStringParameter(FName InName, const FString& InValue)
{
	Parameters.AddStringParameter(InName, InValue);
}

void UParameterModule::SetStringParameter(FName InName, const FString& InValue)
{
	Parameters.SetStringParameter(InName, InValue);
}

FString UParameterModule::GetStringParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetStringParameter(InName, bEnsured);
}

TArray<FString> UParameterModule::GetStringParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetStringParameters(InName, bEnsured);
}

void UParameterModule::AddTextParameter(FName InName, const FText InValue)
{
	Parameters.AddTextParameter(InName, InValue);
}

void UParameterModule::SetTextParameter(FName InName, const FText InValue)
{
	Parameters.SetTextParameter(InName, InValue);
}

FText UParameterModule::GetTextParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetTextParameter(InName, bEnsured);
}

TArray<FText> UParameterModule::GetTextParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetTextParameters(InName, bEnsured);
}

void UParameterModule::AddBooleanParameter(FName InName, bool InValue)
{
	Parameters.AddBooleanParameter(InName, InValue);
}

void UParameterModule::SetBooleanParameter(FName InName, bool InValue)
{
	Parameters.SetBooleanParameter(InName, InValue);
}

bool UParameterModule::GetBooleanParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetBooleanParameter(InName, bEnsured);
}

TArray<bool> UParameterModule::GetBooleanParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetBooleanParameters(InName, bEnsured);
}

void UParameterModule::AddVectorParameter(FName InName, FVector InValue)
{
	Parameters.AddVectorParameter(InName, InValue);
}

void UParameterModule::SetVectorParameter(FName InName, FVector InValue)
{
	Parameters.SetVectorParameter(InName, InValue);
}

FVector UParameterModule::GetVectorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetVectorParameter(InName, bEnsured);
}

TArray<FVector> UParameterModule::GetVectorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetVectorParameters(InName, bEnsured);
}

void UParameterModule::AddRotatorParameter(FName InName, FRotator InValue)
{
	Parameters.AddRotatorParameter(InName, InValue);
}

void UParameterModule::SetRotatorParameter(FName InName, FRotator InValue)
{
	Parameters.SetRotatorParameter(InName, InValue);
}

FRotator UParameterModule::GetRotatorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetRotatorParameter(InName, bEnsured);
}

TArray<FRotator> UParameterModule::GetRotatorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetRotatorParameters(InName, bEnsured);
}

void UParameterModule::AddColorParameter(FName InName, const FColor& InValue)
{
	Parameters.AddColorParameter(InName, InValue);
}

void UParameterModule::SetColorParameter(FName InName, const FColor& InValue)
{
	Parameters.SetColorParameter(InName, InValue);
}

FColor UParameterModule::GetColorParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetColorParameter(InName, bEnsured);
}

TArray<FColor> UParameterModule::GetColorParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetColorParameters(InName, bEnsured);
}

void UParameterModule::AddClassParameter(FName InName, UClass* InValue)
{
	Parameters.AddClassParameter(InName, InValue);
}

void UParameterModule::SetClassParameter(FName InName, UClass* InValue)
{
	Parameters.SetClassParameter(InName, InValue);
}

UClass* UParameterModule::GetClassParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetClassParameter(InName, bEnsured);
}

TArray<UClass*> UParameterModule::GetClassParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetClassParameters(InName, bEnsured);
}

void UParameterModule::AddObjectParameter(FName InName, UObject* InValue)
{
	Parameters.AddObjectParameter(InName, InValue);
}

void UParameterModule::SetObjectParameter(FName InName, UObject* InValue)
{
	Parameters.SetObjectParameter(InName, InValue);
}

UObject* UParameterModule::GetObjectParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetObjectParameter(InName, bEnsured);
}

TArray<UObject*> UParameterModule::GetObjectParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetObjectParameters(InName, bEnsured);
}

void UParameterModule::AddPointerParameter(FName InName, void* InValue)
{
	Parameters.AddPointerParameter(InName, InValue);
}

void UParameterModule::SetPointerParameter(FName InName, void* InValue)
{
	Parameters.SetPointerParameter(InName, InValue);
}

void* UParameterModule::GetPointerParameter(FName InName, bool bEnsured) const
{
	return Parameters.GetPointerParameter(InName, bEnsured);
}

TArray<void*> UParameterModule::GetPointerParameters(FName InName, bool bEnsured) const
{
	return Parameters.GetPointerParameters(InName, bEnsured);
}

void UParameterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UParameterModule, Parameters);
}
