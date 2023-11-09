// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModuleStatics.h"

#include "Step/StepModule.h"
#include "Step/Base/StepBase.h"

UStepBase* UStepModuleStatics::GetCurrentStep()
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		return StepModule->GetCurrentStep();
	}
	return nullptr;
}

UStepBase* UStepModuleStatics::GetCurrentRootStep()
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		return StepModule->GetCurrentRootStep();
	}
	return nullptr;
}

UStepBase* UStepModuleStatics::GetStepByGUID(const FString& InStepGUID)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		return StepModule->GetStepByGUID(InStepGUID);
	}
	return nullptr;
}

void UStepModuleStatics::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->StartStep(InRootStepIndex, bSkipSteps);
	}
}

void UStepModuleStatics::EndStep(bool bRestoreSteps)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->EndStep(bRestoreSteps);
	}
}

void UStepModuleStatics::RestoreStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->RestoreStep(InStep);
	}
}

void UStepModuleStatics::EnterStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->EnterStep(InStep);
	}
}

void UStepModuleStatics::RefreshStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->RefreshStep(InStep);
	}
}

void UStepModuleStatics::GuideStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->GuideStep(InStep);
	}
}

void UStepModuleStatics::ExecuteStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->ExecuteStep(InStep);
	}
}

void UStepModuleStatics::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->CompleteStep(InStep, InStepExecuteResult);
	}
}

void UStepModuleStatics::LeaveStep(UStepBase* InStep)
{
	if(UStepModule* StepModule = UStepModule::Get())
	{
		StepModule->LeaveStep(InStep);
	}
}
