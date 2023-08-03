// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Step/StepModule.h"
#include "Step/StepModuleNetworkComponent.h"
#include "Step/Base/StepBase.h"

UStepBase* UStepModuleBPLibrary::GetCurrentStep()
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		return StepModule->GetCurrentStep();
	}
	return nullptr;
}

UStepBase* UStepModuleBPLibrary::GetCurrentRootStep()
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		return StepModule->GetCurrentRootStep();
	}
	return nullptr;
}

UStepBase* UStepModuleBPLibrary::GetStepByGUID(const FString& InStepGUID)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		return StepModule->GetStepByGUID(InStepGUID);
	}
	return nullptr;
}

void UStepModuleBPLibrary::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->StartStep(InRootStepIndex, bSkipSteps);
	}
}

void UStepModuleBPLibrary::EndStep(bool bRestoreSteps)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->EndStep(bRestoreSteps);
	}
}

void UStepModuleBPLibrary::RestoreStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->RestoreStep(InStep);
	}
}

void UStepModuleBPLibrary::EnterStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->EnterStep(InStep);
	}
}

void UStepModuleBPLibrary::RefreshStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->RefreshStep(InStep);
	}
}

void UStepModuleBPLibrary::GuideStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->GuideStep(InStep);
	}
}

void UStepModuleBPLibrary::ExecuteStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->ExecuteStep(InStep);
	}
}

void UStepModuleBPLibrary::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->CompleteStep(InStep, InStepExecuteResult);
	}
}

void UStepModuleBPLibrary::LeaveStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AStepModule::Get())
	{
		StepModule->LeaveStep(InStep);
	}
}
