// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Procedure/ProcedureModuleBPLibrary.h"

IMPLEMENTATION_MODULE(AProcedureModule)

// ParamSets default values
AProcedureModule::AProcedureModule()
{
	ModuleName = FName("ProcedureModule");

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>();

	bAutoSwitchFirst = false;

	FirstProcedure = nullptr;
	CurrentProcedure = nullptr;
}

AProcedureModule::~AProcedureModule()
{
	TERMINATION_MODULE(AProcedureModule)
}

#if WITH_EDITOR
void AProcedureModule::OnGenerate()
{
	Super::OnGenerate();
}

void AProcedureModule::OnDestroy()
{
	Super::OnDestroy();

	ClearAllProcedure();
}
#endif

void AProcedureModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AProcedureModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
	
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

void AProcedureModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(CurrentProcedure)
	{
		if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
		{
			CurrentProcedure->OnRefresh();
		}
	}
}

void AProcedureModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AProcedureModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AProcedureModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AProcedureModule::SwitchProcedure(UProcedureBase* InProcedure)
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

void AProcedureModule::SwitchProcedureByIndex(int32 InIndex)
{
	if(HasProcedureByIndex(InIndex))
	{
		SwitchProcedure(GetProcedureByIndex<UProcedureBase>(InIndex));
	}
	else
	{
		WHDebug(FString::Printf(TEXT("切换流程失败，不存在指定索引的流程: %d"), InIndex), EDebugMode::All, EDC_Procedure, EDV_Warning, FColor::Red, 5.f);
	}
}

void AProcedureModule::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(HasProcedureByClass<UProcedureBase>(InClass))
	{
		SwitchProcedure(GetProcedureByClass<UProcedureBase>(InClass));
	}
	else
	{
		WHDebug(FString::Printf(TEXT("切换流程失败，不存在指定类型的流程: %s"), InClass ? *InClass->GetName() : TEXT("None")), EDebugMode::All, EDC_Procedure, EDV_Warning, FColor::Red, 5.f);
	}
}

void AProcedureModule::SwitchFirstProcedure()
{
	if(FirstProcedure)
	{
		SwitchProcedure(FirstProcedure);
	}
}

void AProcedureModule::SwitchLastProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex > 0)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex - 1);
	}
}

void AProcedureModule::SwitchNextProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex < Procedures.Num() - 1)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex + 1);
	}
}

void AProcedureModule::GuideCurrentProcedure()
{
	if(CurrentProcedure)
	{
		CurrentProcedure->Guide();
	}
}

void AProcedureModule::ClearAllProcedure()
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

bool AProcedureModule::HasProcedureByIndex(int32 InIndex) const
{
	return Procedures.IsValidIndex(InIndex);
}

UProcedureBase* AProcedureModule::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass) const
{
	return GetProcedureByIndex<UProcedureBase>(InIndex);
}

bool AProcedureModule::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
	return HasProcedureByClass<UProcedureBase>(InClass);
}

UProcedureBase* AProcedureModule::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
	return GetProcedureByClass<UProcedureBase>(InClass);
}

bool AProcedureModule::IsCurrentProcedure(UProcedureBase* InProcedure) const
{
	return InProcedure == CurrentProcedure;
}

bool AProcedureModule::IsCurrentProcedureIndex(int32 InIndex) const
{
	return CurrentProcedure && CurrentProcedure->ProcedureIndex == InIndex;
}

#if WITH_EDITOR

void AProcedureModule::GenerateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		Procedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
	}
}

void AProcedureModule::UpdateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		Procedures[i]->ProcedureIndex = i;
		Procedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}
#endif
