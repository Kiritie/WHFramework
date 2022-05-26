// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Step/Base/StepBase.h"
#include "Step/Base/RootStepBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Step/EventHandle_EndStep.h"
#include "Event/Handle/Step/EventHandle_StartStep.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Step/StepModuleBPLibrary.h"
#include "Step/StepModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

// ParamSets default values
AStepModule::AStepModule()
{
	ModuleName = FName("StepModule");

	bAutoStart = false;

	CurrentRootStepIndex = -1;
	RootSteps = TArray<URootStepBase*>();

	FirstStep = nullptr;
	CurrentStep = nullptr;
	StepModuleState = EStepModuleState::None;

	GlobalStepExecuteType = EStepExecuteType::None;
	GlobalStepLeaveType = EStepLeaveType::None;
	GlobalStepCompleteType = EStepCompleteType::None;
}

#if WITH_EDITOR
void AStepModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AStepModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	ClearAllStep();
}
#endif

void AStepModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	if(!FirstStep && RootSteps.Num() > 0)
	{
		FirstStep = RootSteps[0];
	}
	for(auto Iter : RootSteps)
	{
		if(Iter)
		{
			Iter->OnInitialize();
		}
	}
}

void AStepModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	if(bAutoStart)
	{
		StartStep(-1, true);
	}
}

void AStepModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(StepModuleState == EStepModuleState::Running)
	{
		if(CurrentStep)
		{
			if(!CurrentStep->IsA(URootStepBase::StaticClass()))
			{
				if(CurrentStep->GetStepState() != EStepState::Leaved)
				{
					CurrentStep->Refresh();
				}
				else if(CurrentStep->ParentStep)
				{
					if(CurrentStep->ParentStep->GetStepState() != EStepState::Leaved)
					{
						CurrentStep->ParentStep->Refresh();
					}
					else if(CurrentStep->ParentStep->IsA(URootStepBase::StaticClass()))
					{
						StartStep(CurrentStep->ParentStep->StepIndex + 1, false);
					}
				}
			}
			else
			{
				if(CurrentStep->GetStepState() != EStepState::Leaved)
				{
					CurrentStep->Refresh();
				}
				else
				{
					StartStep(CurrentStep->StepIndex + 1, false);
				}
			}
		}
	}
}

void AStepModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AStepModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AStepModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	EndStep();
}

void AStepModule::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	InRootStepIndex = InRootStepIndex != -1 ? InRootStepIndex : FirstStep && FirstStep->IsA(URootStepBase::StaticClass()) ? FirstStep->StepIndex : -1;
	if(InRootStepIndex != -1)
	{
		if(RootSteps.IsValidIndex(InRootStepIndex))
		{
			if(StepModuleState != EStepModuleState::Running)
			{
				StepModuleState = EStepModuleState::Running;
				UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartStep::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeInteger(InRootStepIndex)});
			}

            for(int32 i = CurrentRootStepIndex; i <= InRootStepIndex; i++)
            {
            	if(RootSteps.IsValidIndex(i) && RootSteps[i])
            	{
            		if(i == InRootStepIndex)
            		{
            			RootSteps[i]->Enter();
            		}
            		else if(bSkipSteps)
            		{
            			RootSteps[i]->Complete(EStepExecuteResult::Skipped);
            		}
            	}
            }
            CurrentRootStepIndex = InRootStepIndex;
		}
	}
	else if(FirstStep)
	{
		InRootStepIndex = FirstStep->RootStep->StepIndex;
		
		if(RootSteps.IsValidIndex(InRootStepIndex))
		{
			if(StepModuleState != EStepModuleState::Running)
			{
				StepModuleState = EStepModuleState::Running;
				UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartStep::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeInteger(InRootStepIndex)});
			}
		
			if(bSkipSteps)
			{
				for(int32 i = CurrentRootStepIndex; i <= InRootStepIndex; i++)
				{
					if(RootSteps.IsValidIndex(i) && RootSteps[i])
					{
						if(i == InRootStepIndex)
						{
							RootSteps[i]->Enter();
							RootSteps[i]->Execute();
							for(int32 j = 0; j < FirstStep->StepIndex; j++)
							{
								FirstStep->ParentStep->SubSteps[j]->Complete(EStepExecuteResult::Skipped);
							}
						}
						else
						{
							RootSteps[i]->Complete(EStepExecuteResult::Skipped);
						}
					}
				}
			}
			FirstStep->Enter();
		
			CurrentRootStepIndex = InRootStepIndex;
		}
	}
}

void AStepModule::EndStep(bool bRestoreSteps)
{
	if(StepModuleState == EStepModuleState::Running)
	{
		StepModuleState = EStepModuleState::Ended;
		UEventModuleBPLibrary::BroadcastEvent<UEventHandle_EndStep>(EEventNetType::Single, this);
	}

	for(int32 i = CurrentRootStepIndex; i >= 0; i--)
	{
		if(RootSteps.IsValidIndex(i) && RootSteps[i])
		{
			if(i == CurrentRootStepIndex)
			{
				RootSteps[i]->Complete(EStepExecuteResult::Skipped);
			}
			if(bRestoreSteps)
			{
				RootSteps[i]->Restore();
			}
		}
	}
	if(bRestoreSteps)
	{
		CurrentRootStepIndex = -1;
		CurrentStep = nullptr;
	}
}

void AStepModule::RestoreStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() != EStepState::None)
	{
		InStep->OnRestore();
	}
}

void AStepModule::EnterStep(UStepBase* InStep)
{
	if(InStep->ParentStep && !InStep->ParentStep->IsEntered())
	{
		InStep->ParentStep->CurrentSubStepIndex = InStep->StepIndex - 1;
		EnterStep(InStep->ParentStep);
		return;
	}
	if(InStep && InStep->GetStepState() == EStepState::None)
	{
		if(CurrentStep && !CurrentStep->IsParentOf(InStep))
		{
			CurrentStep->Leave();
		}
		InStep->OnEnter(CurrentStep);

		if(InStep->IsA(URootStepBase::StaticClass()))
		{
			CurrentRootStepIndex = InStep->StepIndex;
		}

		CurrentStep = InStep;
	}
}

void AStepModule::RefreshStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() == EStepState::Executing)
	{
		InStep->OnRefresh();
	}
}

void AStepModule::GuideStep(UStepBase* InStep)
{
	if(InStep && InStep->IsEntered())
	{
		InStep->OnGuide();
	}
}

void AStepModule::ExecuteStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() == EStepState::Entered)
	{
		InStep->OnExecute();
	}
}

void AStepModule::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	if(!InStep) return;
	
	if(!InStep->IsCompleted())
	{
		InStep->OnComplete(InStepExecuteResult);
	}
	for(auto Iter : InStep->SubSteps)
	{
		if(Iter) Iter->Complete(InStepExecuteResult);
	}
}

void AStepModule::LeaveStep(UStepBase* InStep)
{
	if(!InStep) return;
	
	if(InStep->GetStepState() != EStepState::Completed)
	{
		InStep->Complete(EStepExecuteResult::Skipped);
	}
	if(InStep->GetStepState() != EStepState::Leaved)
	{
		InStep->OnLeave();
		if(InStep->IsA(URootStepBase::StaticClass()))
		{
			if(InStep->StepIndex == RootSteps.Num() - 1)
			{
				EndStep();
			}
		}
	}
}

void AStepModule::ClearAllStep()
{
	for(auto Iter : RootSteps)
	{
		if(Iter)
		{
			#if(WITH_EDITOR)
			Iter->OnUnGenerate();
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	RootSteps.Empty();

	Modify();
}

bool AStepModule::IsAllStepCompleted()
{
	for(auto Iter : RootSteps)
	{
		if(!Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
void AStepModule::GenerateListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	OutStepListItems = TArray<TSharedPtr<FStepListItem>>();
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		auto Item = MakeShared<FStepListItem>();
		RootSteps[i]->GenerateListItem(Item);
		OutStepListItems.Add(Item);
	}
}

void AStepModule::UpdateListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		RootSteps[i]->StepIndex = i;
		RootSteps[i]->StepHierarchy = 0;
		RootSteps[i]->UpdateListItem(OutStepListItems[i]);
	}
}

void AStepModule::SetRootStepItem(int32 InIndex, URootStepBase* InRootStep)
{
	if(RootSteps.IsValidIndex(InIndex))
	{
		RootSteps[InIndex] = InRootStep;
	}
}
#endif

void AStepModule::SetGlobalStepExecuteType(EStepExecuteType InGlobalStepExecuteType)
{
	if(GlobalStepExecuteType != InGlobalStepExecuteType)
	{
		GlobalStepExecuteType = InGlobalStepExecuteType;
		if(CurrentStep && CurrentStep->GetStepExecuteType() == EStepExecuteType::Automatic && CurrentStep->GetStepState() == EStepState::Entered)
		{
			CurrentStep->Execute();
		}
	}
}

void AStepModule::SetGlobalStepCompleteType(EStepCompleteType InGlobalStepCompleteType)
{
	if(GlobalStepCompleteType != InGlobalStepCompleteType)
	{
		GlobalStepCompleteType = InGlobalStepCompleteType;
		if(CurrentStep && CurrentStep->GetStepCompleteType() == EStepCompleteType::Automatic && CurrentStep->GetStepState() == EStepState::Executing)
		{
			CurrentStep->Complete();
		}
	}
}

void AStepModule::SetGlobalStepLeaveType(EStepLeaveType InGlobalStepLeaveType)
{
	if(GlobalStepLeaveType != InGlobalStepLeaveType)
	{
		GlobalStepLeaveType = InGlobalStepLeaveType;
		if(CurrentStep && CurrentStep->GetStepLeaveType() == EStepLeaveType::Automatic && CurrentStep->GetStepState() == EStepState::Completed)
		{
			CurrentStep->Leave();
		}
	}
}
