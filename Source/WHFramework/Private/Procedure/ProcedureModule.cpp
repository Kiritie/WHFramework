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
#include "Network/NetworkModuleBPLibrary.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

// ParamSets default values
AProcedureModule::AProcedureModule()
{
	ModuleName = FName("ProcedureModule");

	FirstRootProcedureIndex = -1;
	CurrentRootProcedureIndex = -1;

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

	StartProcedure();
}

void AProcedureModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(CurrentProcedure)
	{
		if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
		{
			CurrentProcedure->OnRefresh(DeltaSeconds);
		}
		else if(CurrentProcedure->ParentProcedure)
		{
			if(CurrentProcedure->ParentProcedure->GetProcedureState() != EProcedureState::Leaved)
			{
				CurrentProcedure->ParentProcedure->OnRefresh(DeltaSeconds);
			}
		}
		else if(CurrentProcedure->IsA(URootProcedureBase::StaticClass()))
		{
			StartProcedure(CurrentProcedure->ProcedureIndex + 1);
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

void AProcedureModule::StartProcedure(int32 InRootProcedureIndex, bool bSkipProcedures)
{
	CurrentRootProcedureIndex = InRootProcedureIndex != -1 ? InRootProcedureIndex : FirstRootProcedureIndex;

	if(bSkipProcedures)
	{
		for(int32 i = 0; i < CurrentRootProcedureIndex; i++)
		{
			if(RootProcedures.IsValidIndex(i))
			{
				if(RootProcedures[i])
				{
					RootProcedures[i]->Complete(EProcedureExecuteResult::Skipped);
				}
			}
		}
	}
	if(RootProcedures.IsValidIndex(CurrentRootProcedureIndex))
	{
		if(RootProcedures[CurrentRootProcedureIndex])
		{
			RootProcedures[CurrentRootProcedureIndex]->Enter();
		}
	}
}

void AProcedureModule::EndProcedure(bool bRestoreProcedures)
{
	if(CurrentProcedure)
	{
		CurrentProcedure->Complete(EProcedureExecuteResult::Skipped);
		CurrentProcedure->Leave();
		CurrentProcedure = nullptr;
	}
	CurrentRootProcedureIndex = -1;
	if(bRestoreProcedures)
	{
		for(auto Iter : RootProcedures)
		{
			if(Iter)
			{
				Iter->Restore();
			}
		}
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
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Entered)
	{
		if(CurrentProcedure && !CurrentProcedure->IsParentOf(InProcedure))
		{
			CurrentProcedure->OnLeave();
		}
		InProcedure->OnEnter(CurrentProcedure);

		CurrentProcedure = InProcedure;

		if(CurrentProcedure->IsA(URootProcedureBase::StaticClass()))
		{
			CurrentRootProcedureIndex = CurrentProcedure->ProcedureIndex;
		}
	}
}

void AProcedureModule::GuideProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Leaved)
	{
		InProcedure->OnGuide();
	}
}

void AProcedureModule::ExecuteProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Executing)
	{
		InProcedure->OnExecute();
	}
}

void AProcedureModule::CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Completed)
	{
		InProcedure->OnComplete(InProcedureExecuteResult);
		for(auto Iter : InProcedure->SubProcedures)
		{
			if(Iter && Iter->GetProcedureState() != EProcedureState::Completed)
			{
				Iter->OnComplete(InProcedureExecuteResult);
			}
		}
	}
}

void AProcedureModule::LeaveProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && /*(InProcedure == CurrentProcedure || InProcedure == CurrentProcedure->ParentProcedure) && */InProcedure->GetProcedureState() != EProcedureState::Leaved)
	{
		CurrentProcedure->OnLeave();
	}
}

#if WITH_EDITOR
void AProcedureModule::OpenProcedureEditor()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("WHProcedureEditor"));
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
		RootProcedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}
#endif