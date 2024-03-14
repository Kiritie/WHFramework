// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Procedure/EventHandle_SwitchProcedure.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

IMPLEMENTATION_MODULE(UProcedureModule)

// ParamSets default values
UProcedureModule::UProcedureModule()
{
	ModuleName = FName("ProcedureModule");
	ModuleDisplayName = FText::FromString(TEXT("Procedure Module"));

	ModuleNetworkComponent = UProcedureModuleNetworkComponent::StaticClass();

	bAutoSwitchFirst = false;

	DefaultAsset = nullptr;
	CurrentAsset = nullptr;
	CurrentProcedure = nullptr;
}

UProcedureModule::~UProcedureModule()
{
	TERMINATION_MODULE(UProcedureModule)
}

#if WITH_EDITOR
void UProcedureModule::OnGenerate()
{
	Super::OnGenerate();
}

void UProcedureModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UProcedureModule)
}
#endif

void UProcedureModule::OnInitialize()
{
	Super::OnInitialize();

	UEventModuleStatics::SubscribeEvent<UEventHandle_SwitchProcedure>(this, FName("OnSwitchProcedure"));

	if(DefaultAsset)
	{
		SetCurrentAsset(DefaultAsset);
	}
}

void UProcedureModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bAutoSwitchFirst)
		{
			SwitchFirstProcedure();
		}
	}
}

void UProcedureModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	if(CurrentProcedure)
	{
		if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
		{
			CurrentProcedure->OnRefresh();
		}
	}
}

void UProcedureModule::OnPause()
{
	Super::OnPause();
}

void UProcedureModule::OnUnPause()
{
	Super::OnUnPause();
}

void UProcedureModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

FString UProcedureModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentProcedure: %s"), CurrentProcedure ? *CurrentProcedure->ProcedureDisplayName.ToString() : TEXT("None"));
}

void UProcedureModule::OnSwitchProcedure(UObject* InSender, UEventHandle_SwitchProcedure* InEventHandle)
{
	SwitchProcedureByClass(InEventHandle->ProcedureClass);
}

void UProcedureModule::SwitchProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure)
	{
		if(InProcedure->GetProcedureState() != EProcedureState::Entered)
		{
			if(CurrentProcedure)
			{
				CurrentProcedure->OnLeave(InProcedure);
			}
			UProcedureBase* LastProcedure = CurrentProcedure;
			CurrentProcedure = InProcedure;
			InProcedure->OnEnter(LastProcedure);
		}
	}
	else if(CurrentProcedure)
	{
		CurrentProcedure->OnLeave(nullptr);
		CurrentProcedure = nullptr;
	}
}

void UProcedureModule::SwitchProcedureByIndex(int32 InIndex)
{
	if(HasProcedureByIndex(InIndex))
	{
		SwitchProcedure(GetProcedureByIndex<UProcedureBase>(InIndex));
	}
	else
	{
		WHDebug(FString::Printf(TEXT("切换流程失败，不存在指定索引的流程: %d"), InIndex), EDM_All, EDC_Procedure, EDV_Warning, FColor::Red, 5.f);
	}
}

void UProcedureModule::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(HasProcedureByClass<UProcedureBase>(InClass))
	{
		SwitchProcedure(GetProcedureByClass<UProcedureBase>(InClass));
	}
	else
	{
		SwitchProcedure(nullptr);
	}
}

void UProcedureModule::SwitchFirstProcedure()
{
	if(GetFirstProcedure())
	{
		SwitchProcedure(GetFirstProcedure());
	}
}

void UProcedureModule::SwitchLastProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex > 0)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex - 1);
	}
}

void UProcedureModule::SwitchNextProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex < GetProcedures().Num() - 1)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex + 1);
	}
}

void UProcedureModule::GuideCurrentProcedure()
{
	if(CurrentProcedure)
	{
		CurrentProcedure->Guide();
	}
}

void UProcedureModule::SetCurrentAsset(UProcedureAsset* InProcedureAsset, bool bInAutoSwitchFirst)
{
	if(!InProcedureAsset || (CurrentAsset && InProcedureAsset == CurrentAsset->SourceObject)) return;

	CurrentAsset = DuplicateObject<UProcedureAsset>(InProcedureAsset, this);
	CurrentAsset->Initialize(InProcedureAsset);

	WHDebug(FString::Printf(TEXT("切换流程源: %s"), !CurrentAsset->DisplayName.IsEmpty() ? *CurrentAsset->DisplayName.ToString() : *CurrentAsset->GetName()), EDM_All, EDC_Procedure, EDV_Log, FColor::Green, 5.f);

	if(bInAutoSwitchFirst)
	{
		SwitchFirstProcedure();
	}
}

bool UProcedureModule::HasProcedureByIndex(int32 InIndex) const
{
	return GetProcedures().IsValidIndex(InIndex);
}

UProcedureBase* UProcedureModule::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass) const
{
	return GetProcedureByIndex<UProcedureBase>(InIndex);
}

bool UProcedureModule::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
	return HasProcedureByClass<UProcedureBase>(InClass);
}

UProcedureBase* UProcedureModule::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
	return GetProcedureByClass<UProcedureBase>(InClass);
}

bool UProcedureModule::IsCurrentProcedure(UProcedureBase* InProcedure) const
{
	return InProcedure == CurrentProcedure;
}

bool UProcedureModule::IsCurrentProcedureIndex(int32 InIndex) const
{
	return CurrentProcedure && CurrentProcedure->ProcedureIndex == InIndex;
}
