// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModuleStatics.h"

#include "Step/StepModule.h"
#include "Step/Base/StepBase.h"

UStepBase* UStepModuleStatics::GetCurrentStep()
{
	return UStepModule::Get().GetCurrentStep();
}

UStepBase* UStepModuleStatics::GetCurrentRootStep()
{
	return UStepModule::Get().GetCurrentRootStep();
}

UStepBase* UStepModuleStatics::GetStepByGUID(const FString& InStepGUID)
{
	return UStepModule::Get().GetStepByGUID(InStepGUID);
}

void UStepModuleStatics::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	UStepModule::Get().StartStep(InRootStepIndex, bSkipSteps);
}

void UStepModuleStatics::EndStep(bool bRestoreSteps)
{
	UStepModule::Get().EndStep(bRestoreSteps);
}

void UStepModuleStatics::RestoreStep(UStepBase* InStep)
{
	UStepModule::Get().RestoreStep(InStep);
}

void UStepModuleStatics::EnterStep(UStepBase* InStep)
{
	UStepModule::Get().EnterStep(InStep);
}

void UStepModuleStatics::RefreshStep(UStepBase* InStep)
{
	UStepModule::Get().RefreshStep(InStep);
}

void UStepModuleStatics::GuideStep(UStepBase* InStep)
{
	UStepModule::Get().GuideStep(InStep);
}

void UStepModuleStatics::ExecuteStep(UStepBase* InStep)
{
	UStepModule::Get().ExecuteStep(InStep);
}

void UStepModuleStatics::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	UStepModule::Get().CompleteStep(InStep, InStepExecuteResult);
}

void UStepModuleStatics::LeaveStep(UStepBase* InStep)
{
	UStepModule::Get().LeaveStep(InStep);
}
