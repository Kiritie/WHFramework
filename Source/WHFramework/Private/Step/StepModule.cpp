// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModule.h"

#include "Main/MainModule.h"
#include "Step/Base/StepBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Step/EventHandle_StepEnded.h"
#include "Event/Handle/Step/EventHandle_StepStarted.h"
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

	DefaultAsset = nullptr;
	CurrentAsset = nullptr;
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
}
#endif

void UStepModule::OnInitialize()
{
	Super::OnInitialize();

	if(DefaultAsset)
	{
		SetCurrentAsset(DefaultAsset);
	}
}

void UStepModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

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

FString UStepModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentStep: %s"), CurrentStep ? *CurrentStep->StepDisplayName.ToString() : TEXT("None"));
}

void UStepModule::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	InRootStepIndex = InRootStepIndex != -1 ? InRootStepIndex : GetFirstStep() ? GetFirstStep()->StepIndex : -1;
	if(InRootStepIndex != -1)
	{
		if(GetRootSteps().IsValidIndex(InRootStepIndex))
		{
			if(StepModuleState != EStepModuleState::Running)
			{
				StepModuleState = EStepModuleState::Running;
				UEventModuleStatics::BroadcastEvent(UEventHandle_StepStarted::StaticClass(), this, {InRootStepIndex});
			}

            for(int32 i = CurrentRootStepIndex; i <= InRootStepIndex; i++)
            {
            	if(GetRootSteps().IsValidIndex(i) && GetRootSteps()[i])
            	{
            		if(i == InRootStepIndex)
            		{
            			GetRootSteps()[i]->Enter();
            		}
            		else if(bSkipSteps)
            		{
            			GetRootSteps()[i]->Complete(EStepExecuteResult::Skipped);
            		}
            	}
            }
            CurrentRootStepIndex = InRootStepIndex;
		}
	}
	else if(GetFirstStep())
	{
		InRootStepIndex = GetFirstStep()->RootStep->StepIndex;
		
		if(GetRootSteps().IsValidIndex(InRootStepIndex))
		{
			if(StepModuleState != EStepModuleState::Running)
			{
				StepModuleState = EStepModuleState::Running;
				UEventModuleStatics::BroadcastEvent(UEventHandle_StepStarted::StaticClass(), this, {InRootStepIndex});
			}
		
			if(bSkipSteps)
			{
				for(int32 i = CurrentRootStepIndex; i <= InRootStepIndex; i++)
				{
					if(GetRootSteps().IsValidIndex(i) && GetRootSteps()[i])
					{
						if(i == InRootStepIndex)
						{
							GetRootSteps()[i]->Enter();
							GetRootSteps()[i]->Execute();
							for(int32 j = 0; j < GetFirstStep()->StepIndex; j++)
							{
								GetFirstStep()->ParentStep->SubSteps[j]->Complete(EStepExecuteResult::Skipped);
							}
						}
						else
						{
							GetRootSteps()[i]->Complete(EStepExecuteResult::Skipped);
						}
					}
				}
			}
			GetFirstStep()->Enter();
		
			CurrentRootStepIndex = InRootStepIndex;
		}
	}
}

void UStepModule::EndStep(bool bRestoreSteps)
{
	if(StepModuleState == EStepModuleState::Running)
	{
		StepModuleState = EStepModuleState::Ended;
		UEventModuleStatics::BroadcastEvent<UEventHandle_StepEnded>(this);
	}

	for(int32 i = CurrentRootStepIndex; i >= 0; i--)
	{
		if(GetRootSteps().IsValidIndex(i) && GetRootSteps()[i])
		{
			if(i == CurrentRootStepIndex)
			{
				GetRootSteps()[i]->Complete(EStepExecuteResult::Skipped);
			}
			if(bRestoreSteps)
			{
				GetRootSteps()[i]->Restore();
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
			if(InStep->StepIndex == GetRootSteps().Num() - 1)
			{
				EndStep();
			}
		}
	}
}

bool UStepModule::IsAllStepCompleted()
{
	for(auto Iter : GetRootSteps())
	{
		if(!Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

void UStepModule::SetCurrentAsset(UStepAsset* InStepAsset, bool bInAutoStartFirst)
{
	if(!InStepAsset || (CurrentAsset && InStepAsset == CurrentAsset->SourceObject)) return;

	CurrentAsset = DuplicateObject<UStepAsset>(InStepAsset, this);
	CurrentAsset->Initialize(InStepAsset);

	WHDebug(FString::Printf(TEXT("切换步骤源: %s"), !CurrentAsset->DisplayName.IsEmpty() ? *CurrentAsset->DisplayName.ToString() : *CurrentAsset->GetName()), EDM_All, EDC_Step, EDV_Log, FColor::Green, 5.f);

	if(bInAutoStartFirst)
	{
		StartStep(-1, true);
	}
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
