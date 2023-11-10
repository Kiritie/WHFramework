// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Procedure/ProcedureModuleStatics.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

IMPLEMENTATION_MODULE(UProcedureModule)

// ParamSets default values
UProcedureModule::UProcedureModule()
{
	ModuleName = FName("ProcedureModule");
	ModuleDisplayName = FText::FromString(TEXT("Procedure Module"));

	ModuleNetworkComponent = UProcedureModuleNetworkComponent::StaticClass();

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>();

	bAutoSwitchFirst = false;

	FirstProcedure = nullptr;
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

	ClearAllProcedure();
}
#endif

void UProcedureModule::OnInitialize()
{
	Super::OnInitialize();
}

void UProcedureModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	
	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(!FirstProcedure && Procedures.Num() > 0)
		{
			FirstProcedure = Procedures[0];
		}
		for(auto Iter : Procedures)
		{
			if(Iter)
			{
				Iter->OnInitialize();
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bAutoSwitchFirst && FirstProcedure)
		{
			SwitchProcedure(FirstProcedure);
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

void UProcedureModule::SwitchProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Entered)
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
		WHDebug(FString::Printf(TEXT("切换流程失败，不存在指定类型的流程: %s"), InClass ? *InClass->GetName() : TEXT("None")), EDM_All, EDC_Procedure, EDV_Warning, FColor::Red, 5.f);
	}
}

void UProcedureModule::SwitchFirstProcedure()
{
	if(FirstProcedure)
	{
		SwitchProcedure(FirstProcedure);
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
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex < Procedures.Num() - 1)
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

void UProcedureModule::ClearAllProcedure()
{
	for(auto Iter : Procedures)
	{
		if(Iter)
		{
#if(WITH_EDITOR)
			Iter->OnUnGenerate();
#else
			Iter->ConditionalBeginDestroy();
#endif
		}
	}
	
	Procedures.Empty();

	ProcedureMap.Empty();

	Modify();
}

bool UProcedureModule::HasProcedureByIndex(int32 InIndex) const
{
	return Procedures.IsValidIndex(InIndex);
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

#if WITH_EDITOR
void UProcedureModule::GenerateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		Procedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
	}
}

void UProcedureModule::UpdateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		Procedures[i]->ProcedureIndex = i;
		Procedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}
#endif
