// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModule.h"

#include "Main/MainModule.h"
#include "Step/Base/StepBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Step/EventHandle_EndStep.h"
#include "Event/Handle/Step/EventHandle_StartStep.h"
#include "Step/StepModuleNetworkComponent.h"

IMPLEMENTATION_MODULE(UStepModule)

// ParamSets default values
UStepModule::UStepModule()
{
	ModuleName = FName("StepModule");
	ModuleDisplayName = FText::FromString(TEXT("Step Module"));

	ModuleNetworkComponent = UStepModuleNetworkComponent::StaticClass();

	bAutoStartFirst = false;

	CurrentRootStepIndex = -1;
	RootSteps = TArray<UStepBase*>();

	FirstStep = nullptr;
	CurrentStep = nullptr;
	StepModuleState = EStepModuleState::None;

	GlobalStepExecuteType = EStepExecuteType::None;
	GlobalStepLeaveType = EStepLeaveType::None;
	GlobalStepCompleteType = EStepCompleteType::None;
}

UStepModule::~UStepModule()
{
	TERMINATION_MODULE(UStepModule)
}

#if WITH_EDITOR
void UStepModule::OnGenerate()
{
	Super::OnGenerate();
}

void UStepModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UStepModule)

	ClearAllStep();
}
#endif

void UStepModule::OnInitialize()
{
	Super::OnInitialize();
}

void UStepModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
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
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bAutoStartFirst)
		{
			StartStep(-1, true);
		}
	}
}

void UStepModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	if(StepModuleState == EStepModuleState::Running)
	{
		if(CurrentStep)
		{
			if(!CurrentStep->IsRootStep())
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
					else if(CurrentStep->ParentStep->IsRootStep())
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

void UStepModule::OnPause()
{
	Super::OnPause();
}

void UStepModule::OnUnPause()
{
	Super::OnUnPause();
}

void UStepModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		EndStep();
	}
}

void UStepModule::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	InRootStepIndex = InRootStepIndex != -1 ? InRootStepIndex : FirstStep && FirstStep->IsA<UStepBase>() ? FirstStep->StepIndex : -1;
	if(InRootStepIndex != -1)
	{
		if(RootSteps.IsValidIndex(InRootStepIndex))
		{
			if(StepModuleState != EStepModuleState::Running)
			{
				StepModuleState = EStepModuleState::Running;
				UEventModuleStatics::BroadcastEvent(UEventHandle_StartStep::StaticClass(), EEventNetType::Single, this, {InRootStepIndex});
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
				UEventModuleStatics::BroadcastEvent(UEventHandle_StartStep::StaticClass(), EEventNetType::Single, this, {InRootStepIndex});
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

void UStepModule::EndStep(bool bRestoreSteps)
{
	if(StepModuleState == EStepModuleState::Running)
	{
		StepModuleState = EStepModuleState::Ended;
		UEventModuleStatics::BroadcastEvent<UEventHandle_EndStep>(EEventNetType::Single, this);
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

void UStepModule::RestoreStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() != EStepState::None)
	{
		InStep->OnRestore();
	}
}

void UStepModule::EnterStep(UStepBase* InStep)
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

		if(InStep->IsA<UStepBase>())
		{
			CurrentRootStepIndex = InStep->StepIndex;
		}

		CurrentStep = InStep;
	}
}

void UStepModule::RefreshStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() == EStepState::Executing)
	{
		InStep->OnRefresh();
	}
}

void UStepModule::GuideStep(UStepBase* InStep)
{
	if(InStep && InStep->IsEntered())
	{
		InStep->OnGuide();
	}
}

void UStepModule::ExecuteStep(UStepBase* InStep)
{
	if(InStep && InStep->GetStepState() == EStepState::Entered)
	{
		InStep->OnExecute();
	}
}

void UStepModule::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
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

void UStepModule::LeaveStep(UStepBase* InStep)
{
	if(!InStep) return;
	
	if(InStep->GetStepState() != EStepState::Completed)
	{
		InStep->Complete(EStepExecuteResult::Skipped);
	}
	if(InStep->GetStepState() != EStepState::Leaved)
	{
		InStep->OnLeave();
		if(InStep->IsA<UStepBase>())
		{
			if(InStep->StepIndex == RootSteps.Num() - 1)
			{
				EndStep();
			}
		}
	}
}

void UStepModule::ClearAllStep()
{
	for(auto Iter : RootSteps)
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
	
	RootSteps.Empty();

	StepMap.Empty();

	Modify();
}

bool UStepModule::IsAllStepCompleted()
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

void UStepModule::SetGlobalStepExecuteType(EStepExecuteType InGlobalStepExecuteType)
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

void UStepModule::SetGlobalStepCompleteType(EStepCompleteType InGlobalStepCompleteType)
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

void UStepModule::SetGlobalStepLeaveType(EStepLeaveType InGlobalStepLeaveType)
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

#if WITH_EDITOR
void UStepModule::GenerateListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	OutStepListItems = TArray<TSharedPtr<FStepListItem>>();
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		auto Item = MakeShared<FStepListItem>();
		RootSteps[i]->GenerateListItem(Item);
		OutStepListItems.Add(Item);
	}
}

void UStepModule::UpdateListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		RootSteps[i]->StepIndex = i;
		RootSteps[i]->StepHierarchy = 0;
		RootSteps[i]->UpdateListItem(OutStepListItems[i]);
	}
}
#endif
