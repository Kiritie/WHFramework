// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Procedure/Base/ProcedureBase.h"
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

	Assets = TArray<UProcedureAsset*>();
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

	UEventModuleStatics::SubscribeEvent<UEventHandle_SwitchProcedure>(this, GET_FUNCTION_NAME_THISCLASS(OnSwitchProcedure));

	if(DefaultAsset)
	{
		DefaultAsset = DefaultAsset->Duplicate<UProcedureAsset>();
		AddAsset(DefaultAsset);
	}
}

void UProcedureModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultAsset)
		{
			SwitchAsset(DefaultAsset);
		}
	}
}

void UProcedureModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

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

UProcedureAsset* UProcedureModule::GetAsset(UProcedureAsset* InAsset) const
{
	for(auto Iter : Assets)
	{
		if(Iter->SourceObject == InAsset->SourceObject)
		{
			return Iter;
		}
	}
	return nullptr;
}

void UProcedureModule::AddAsset(UProcedureAsset* InAsset)
{
	if(!GetAsset(InAsset))
	{
		Assets.Add(InAsset);
		InAsset->Initialize();
	}
}

void UProcedureModule::RemoveAsset(UProcedureAsset* InAsset)
{
	if(UProcedureAsset* Asset = GetAsset(InAsset))
	{
		Assets.Remove(Asset);
	}
}

void UProcedureModule::SwitchAsset(UProcedureAsset* InAsset)
{
	if(!InAsset || !Assets.Contains(InAsset) || CurrentAsset == InAsset) return;

	CurrentAsset = InAsset;

	WHDebug(FString::Printf(TEXT("切换流程源: %s"), !CurrentAsset->DisplayName.IsEmpty() ? *CurrentAsset->DisplayName.ToString() : *CurrentAsset->GetName()), EDM_All, EDC_Procedure, EDV_Log, FColor::Green, 5.f);

	if(CurrentAsset->bAutoSwitchFirst)
	{
		SwitchFirstProcedure();
	}
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
	if(InIndex != -1)
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
	else
	{
		SwitchProcedure(nullptr);
	}
}

void UProcedureModule::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(InClass)
	{
		if(HasProcedureByClass<UProcedureBase>(InClass))
		{
			SwitchProcedure(GetProcedureByClass<UProcedureBase>(InClass));
		}
		else
		{
			WHDebug(FString::Printf(TEXT("切换流程失败，不存在指定类型的流程: %s"), *InClass->GetName()), EDM_All, EDC_Procedure, EDV_Warning, FColor::Red, 5.f);
		}
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
