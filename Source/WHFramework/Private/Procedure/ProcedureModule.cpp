// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Procedure/EventHandle_EndProcedure.h"
#include "Event/Handle/Procedure/EventHandle_StartProcedure.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

// ParamSets default values
AProcedureModule::AProcedureModule()
{
	ModuleName = FName("ProcedureModule");

	bAutoStartProcedure = false;
	ProcedureModuleState = EProcedureModuleState::None;

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>(); 

	FirstProcedure = nullptr;
	CurrentProcedure = nullptr;
	CurrentProcedureIndex = 0;
}

#if WITH_EDITOR
void AProcedureModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AProcedureModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	ClearAllProcedure();
}
#endif

void AProcedureModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(auto Iter : Procedures)
	{
		if(Iter)
		{
			Iter->OnInitialize();
		}
	}
}

void AProcedureModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	if(bAutoStartProcedure)
	{
		StartProcedure(-1, true);
	}
}

void AProcedureModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(ProcedureModuleState == EProcedureModuleState::Running)
	{
		if(CurrentProcedure)
		{
			const int32 Index = CurrentProcedureIndex + 1;
			if(Procedures.IsValidIndex(Index))
			{
				UProcedureBase* Procedure = Procedures[Index];
				if(Procedure && Procedure->GetProcedureEnterType() == EProcedureEnterType::Automatic)
				{
					if(Procedure->CheckProcedureCondition(GetCurrentProcedure()))
					{
						Procedure->Enter();
					}
					else
					{
						Procedure->Complete(EProcedureExecuteResult::Skipped);
						CurrentProcedureIndex++;
					}
				}
			}

			if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
			{
				CurrentProcedure->Refresh();
			}
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

void AProcedureModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	EndProcedure();
}

void AProcedureModule::StartProcedure(int32 InProcedureIndex, bool bSkipProcedures)
{
	InProcedureIndex = InProcedureIndex != -1 ? InProcedureIndex : FirstProcedure ? FirstProcedure->ProcedureIndex : 0;

	if(!Procedures.IsValidIndex(InProcedureIndex)) return;
	
	if(ProcedureModuleState != EProcedureModuleState::Running)
	{
		ProcedureModuleState = EProcedureModuleState::Running;
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeInteger(CurrentProcedureIndex)});
	}

	for(int32 i = CurrentProcedureIndex; i <= InProcedureIndex; i++)
	{
		if(Procedures.IsValidIndex(i))
		{
			if(Procedures[i])
			{
				if(i == InProcedureIndex)
				{
					Procedures[InProcedureIndex]->Enter();
				}
				else if(bSkipProcedures)
				{
					Procedures[i]->Complete(EProcedureExecuteResult::Skipped);
				}
			}
		}
	}

	CurrentProcedureIndex = InProcedureIndex;
}

void AProcedureModule::EndProcedure(bool bRestoreProcedures)
{
	if(ProcedureModuleState != EProcedureModuleState::Ended)
	{
		ProcedureModuleState = EProcedureModuleState::Ended;
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EndProcedure::StaticClass(), EEventNetType::Single, this);
	}

	for(int32 i = CurrentProcedureIndex; i >= 0; i--)
	{
		if(Procedures.IsValidIndex(i))
		{
			if(Procedures[i])
			{
				if(i == CurrentProcedureIndex)
				{
					Procedures[i]->Complete(EProcedureExecuteResult::Skipped);
				}
				if(bRestoreProcedures)
				{
					Procedures[i]->Restore();
				}
			}
		}
	}
	if(bRestoreProcedures)
	{
		CurrentProcedureIndex = 0;
		CurrentProcedure = nullptr;
	}
}

void AProcedureModule::RestoreProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::None)
	{
		InProcedure->OnRestore();
	}
}

void AProcedureModule::RestoreProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Restore();
	}
}

void AProcedureModule::RestoreProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Restore();
	}
}

void AProcedureModule::EnterProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::None)
	{
		if(CurrentProcedure)
		{
			CurrentProcedure->Leave();
		}
		InProcedure->OnEnter(CurrentProcedure);

		CurrentProcedureIndex = InProcedure->ProcedureIndex;

		CurrentProcedure = InProcedure;
	}
}

void AProcedureModule::EnterProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Enter();
	}
}

void AProcedureModule::EnterProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Enter();
	}
}

void AProcedureModule::RefreshProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::Executing)
	{
		InProcedure->OnRefresh();
	}
}

void AProcedureModule::RefreshProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Refresh();
	}
}

void AProcedureModule::RefreshProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Refresh();
	}
}

void AProcedureModule::GuideProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->IsEntered())
	{
		InProcedure->OnGuide();
	}
}

void AProcedureModule::GuideProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Guide();
	}
}

void AProcedureModule::GuideProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Guide();
	}
}

void AProcedureModule::ExecuteProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::Entered)
	{
		InProcedure->OnExecute();
	}
}

void AProcedureModule::ExecuteProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Execute();
	}
}

void AProcedureModule::ExecuteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Execute();
	}
}

void AProcedureModule::CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(!InProcedure) return;
	
	if(InProcedure->IsEntered() && !InProcedure->IsCompleted())
	{
		InProcedure->OnComplete(InProcedureExecuteResult);
	}
}

void AProcedureModule::CompleteProcedureByIndex(int32 InProcedureIndex, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Complete(InProcedureExecuteResult);
	}
}

void AProcedureModule::CompleteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Complete(InProcedureExecuteResult);
	}
}

void AProcedureModule::LeaveProcedure(UProcedureBase* InProcedure)
{
	if(!InProcedure) return;
	
	if(InProcedure->GetProcedureState() != EProcedureState::Completed)
	{
		InProcedure->Complete(EProcedureExecuteResult::Skipped);
	}
	if(InProcedure->GetProcedureState() != EProcedureState::Leaved)
	{
		InProcedure->OnLeave();
		if(InProcedure->IsA(UProcedureBase::StaticClass()))
		{
			if(InProcedure->ProcedureIndex == Procedures.Num() - 1)
			{
				EndProcedure();
			}
		}
	}
}

void AProcedureModule::LeaveProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		GetProcedureByIndex<UProcedureBase>(InProcedureIndex)->Leave();
	}
}

void AProcedureModule::LeaveProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		GetProcedureByClass<UProcedureBase>(InProcedureClass)->Leave();
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
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	Procedures.Empty();

	Modify();
}

bool AProcedureModule::IsAllProcedureCompleted()
{
	for(auto Iter : Procedures)
	{
		if(!Iter->IsCompleted())
		{
			return false;
		}
	}
	return true;
}

UProcedureBase* AProcedureModule::K2_GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return GetProcedureByIndex<UProcedureBase>(InProcedureIndex);
}

bool AProcedureModule::K2_HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return HasProcedureByClass<UProcedureBase>(InProcedureClass);
}

UProcedureBase* AProcedureModule::K2_GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return GetProcedureByClass<UProcedureBase>(InProcedureClass);
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

void AProcedureModule::SetProcedureItem(int32 InIndex, UProcedureBase* InProcedure)
{
	if(Procedures.IsValidIndex(InIndex))
	{
		Procedures[InIndex] = InProcedure;
	}
}
#endif
