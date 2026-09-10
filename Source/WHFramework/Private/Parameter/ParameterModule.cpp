
// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Events/Parameter/Event_GlobalParameterChanged.h"
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

	bModuleRequired = true;
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
}

void UParameterModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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
}

void UParameterModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FParameterModuleSaveData>();

	for(auto& Iter : SaveData.ParameterSets.Sets)
	{
		SetParameter(Iter.Name, Iter.Parameter);
	}
}

FSaveData* UParameterModule::ToData()
{
	FParameterModuleSaveData& SaveData = GetMutableSaveData<FParameterModuleSaveData>();
	SaveData = FParameterModuleSaveData();
	
	SaveData.ParameterSets = ParameterSets;
	return &SaveData;
}

void UParameterModule::Load_Implementation()
{
	Super::Load_Implementation();

	if(!bModuleAutoSave)
	{
		for(auto& Iter : ParameterSets.Sets)
		{
			UEventModuleStatics::BroadcastEvent<FEventGlobalParameterChanged>(this, { Iter.Name, Iter.Parameter });
		}
	}
}

FString UParameterModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Parameter Count: %d"), ParameterSets.Sets.Num());
}

bool UParameterModule::HasParameter(FName InName, bool bEnsured) const
{
	return ParameterSets.HasParameter(InName, bEnsured);
}

void UParameterModule::SetParameter(FName InName, FParameter InParameter)
{
	ParameterSets.SetParameter(InName, InParameter);
	UEventModuleStatics::BroadcastEvent<FEventGlobalParameterChanged>(this, { InName, InParameter });
}

FParameter UParameterModule::GetParameter(FName InName, bool bEnsured) const
{
	return ParameterSets.GetParameter(InName, bEnsured);
}

TArray<FParameter> UParameterModule::GetParameters(FName InName, bool bEnsured) const
{
	return ParameterSets.GetParameters(InName, bEnsured);
}

void UParameterModule::RemoveParameter(FName InName)
{
	ParameterSets.RemoveParameter(InName);
}

void UParameterModule::RemoveParameters(FName InName)
{
	ParameterSets.RemoveParameters(InName);
}

TArray<FParameterSet> UParameterModule::GetAllParameter()
{
	return ParameterSets.Sets;
}

void UParameterModule::ClearAllParameter()
{
	ParameterSets.ClearAllParameter();
}

void UParameterModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UParameterModule, ParameterSets);
}
