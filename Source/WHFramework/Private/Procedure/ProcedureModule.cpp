// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Base/RootProcedureBase.h"
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

	FirstRootProcedureIndex = 0;
	CurrentRootProcedureIndex = 0;

	RootProcedures = TArray<URootProcedureBase*>();

	GlobalProcedureExecuteType = EProcedureExecuteType::None;
	GlobalProcedureLeaveType = EProcedureLeaveType::None;
	GlobalProcedureCompleteType = EProcedureCompleteType::None;
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

	for(auto Iter : RootProcedures)
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
			if(!CurrentProcedure->IsA(URootProcedureBase::StaticClass()))
			{
				if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
				{
					CurrentProcedure->Refresh();
				}
				else if(CurrentProcedure->ParentProcedure)
				{
					if(CurrentProcedure->ParentProcedure->GetProcedureState() != EProcedureState::Leaved)
					{
						CurrentProcedure->ParentProcedure->Refresh();
					}
					else if(CurrentProcedure->ParentProcedure->IsA(URootProcedureBase::StaticClass()))
					{
						StartProcedure(CurrentProcedure->ParentProcedure->ProcedureIndex + 1, false);
					}
				}
			}
			else
			{
				if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
				{
					CurrentProcedure->Refresh();
				}
				else
				{
					StartProcedure(CurrentProcedure->ProcedureIndex + 1, false);
				}
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

void AProcedureModule::StartProcedure(int32 InRootProcedureIndex, bool bSkipProcedures)
{
	const int32 TargetRootProcedureIndex = InRootProcedureIndex != -1 ? InRootProcedureIndex : FirstRootProcedureIndex;

	if(!RootProcedures.IsValidIndex(TargetRootProcedureIndex)) return;
	
	if(ProcedureModuleState != EProcedureModuleState::Running)
	{
		ProcedureModuleState = EProcedureModuleState::Running;
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeInteger(CurrentRootProcedureIndex)});
	}

	for(int32 i = CurrentRootProcedureIndex; i <= TargetRootProcedureIndex; i++)
	{
		if(RootProcedures.IsValidIndex(i))
		{
			if(RootProcedures[i])
			{
				if(i == TargetRootProcedureIndex)
				{
					RootProcedures[TargetRootProcedureIndex]->Enter();
				}
				else if(bSkipProcedures)
				{
					RootProcedures[i]->Complete(EProcedureExecuteResult::Skipped);
				}
			}
		}
	}

	CurrentRootProcedureIndex = TargetRootProcedureIndex;
}

void AProcedureModule::EndProcedure(bool bRestoreProcedures)
{
	if(ProcedureModuleState != EProcedureModuleState::Ended)
	{
		ProcedureModuleState = EProcedureModuleState::Ended;
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EndProcedure::StaticClass(), EEventNetType::Single, this);
	}

	for(int32 i = CurrentRootProcedureIndex; i >= 0; i--)
	{
		if(RootProcedures.IsValidIndex(i))
		{
			if(RootProcedures[i])
			{
				if(i == CurrentRootProcedureIndex)
				{
					RootProcedures[i]->Complete(EProcedureExecuteResult::Skipped);
				}
				if(bRestoreProcedures)
				{
					RootProcedures[i]->Restore();
				}
			}
		}
	}
	if(bRestoreProcedures)
	{
		CurrentRootProcedureIndex = 0;
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

void AProcedureModule::EnterProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::None)
	{
		if(CurrentProcedure && !CurrentProcedure->IsParentOf(InProcedure))
		{
			CurrentProcedure->Leave();
		}
		InProcedure->OnEnter(CurrentProcedure);

		if(InProcedure->IsA(URootProcedureBase::StaticClass()))
		{
			CurrentRootProcedureIndex = InProcedure->ProcedureIndex;
		}

		CurrentProcedure = InProcedure;
	}
}

void AProcedureModule::RefreshProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::Executing)
	{
		InProcedure->OnRefresh();
	}
}

void AProcedureModule::GuideProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->IsEntered())
	{
		InProcedure->OnGuide();
	}
}

void AProcedureModule::ExecuteProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() == EProcedureState::Entered)
	{
		InProcedure->OnExecute();
	}
}

void AProcedureModule::CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(!InProcedure) return;
	
	if(InProcedure->IsEntered() && !InProcedure->IsCompleted())
	{
		InProcedure->OnComplete(InProcedureExecuteResult);
	}
	for(auto Iter : InProcedure->SubProcedures)
	{
		if(Iter) Iter->Complete(InProcedureExecuteResult);
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
		if(InProcedure->IsA(URootProcedureBase::StaticClass()))
		{
			if(InProcedure->ProcedureIndex == RootProcedures.Num() - 1)
			{
				EndProcedure();
			}
		}
	}
}

void AProcedureModule::ClearAllProcedure()
{
	for(auto Iter : RootProcedures)
	{
		if(Iter)
		{
			#if(WITH_EDITOR)
			Iter->OnUnGenerate();
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	RootProcedures.Empty();

	Modify();
}

bool AProcedureModule::IsAllProcedureCompleted()
{
	for(auto Iter : RootProcedures)
	{
		if(!Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
void AProcedureModule::GenerateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < RootProcedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		RootProcedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
	}
}

void AProcedureModule::UpdateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < RootProcedures.Num(); i++)
	{
		RootProcedures[i]->ProcedureIndex = i;
		RootProcedures[i]->ProcedureHierarchy = 0;
		RootProcedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}

void AProcedureModule::SetRootProcedureItem(int32 InIndex, URootProcedureBase* InRootProcedure)
{
	if(RootProcedures.IsValidIndex(InIndex))
	{
		RootProcedures[InIndex] = InRootProcedure;
	}
}
#endif

void AProcedureModule::SetGlobalProcedureExecuteType(EProcedureExecuteType InGlobalProcedureExecuteType)
{
	if(GlobalProcedureExecuteType != InGlobalProcedureExecuteType)
	{
		GlobalProcedureExecuteType = InGlobalProcedureExecuteType;
		if(CurrentProcedure && CurrentProcedure->GetProcedureExecuteType() == EProcedureExecuteType::Automatic && CurrentProcedure->GetProcedureState() == EProcedureState::Entered)
		{
			CurrentProcedure->Execute();
		}
	}
}

void AProcedureModule::SetGlobalProcedureCompleteType(EProcedureCompleteType InGlobalProcedureCompleteType)
{
	if(GlobalProcedureCompleteType != InGlobalProcedureCompleteType)
	{
		GlobalProcedureCompleteType = InGlobalProcedureCompleteType;
		if(CurrentProcedure && CurrentProcedure->GetProcedureCompleteType() == EProcedureCompleteType::Automatic && CurrentProcedure->GetProcedureState() == EProcedureState::Executing)
		{
			CurrentProcedure->Complete();
		}
	}
}

void AProcedureModule::SetGlobalProcedureLeaveType(EProcedureLeaveType InGlobalProcedureLeaveType)
{
	if(GlobalProcedureLeaveType != InGlobalProcedureLeaveType)
	{
		GlobalProcedureLeaveType = InGlobalProcedureLeaveType;
		if(CurrentProcedure && CurrentProcedure->GetProcedureLeaveType() == EProcedureLeaveType::Automatic && CurrentProcedure->GetProcedureState() == EProcedureState::Completed)
		{
			CurrentProcedure->Leave();
		}
	}
}
