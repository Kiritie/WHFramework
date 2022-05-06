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
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		return StepModule->GetCurrentStep();
	}
	return nullptr;
}

URootStepBase* UStepModuleBPLibrary::GetCurrentRootStep()
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		return StepModule->GetCurrentRootStep();
	}
	return nullptr;
}

void UStepModuleBPLibrary::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->StartStep(InRootStepIndex, bSkipSteps);
	}
}

void UStepModuleBPLibrary::EndStep(bool bRestoreSteps)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->EndStep(bRestoreSteps);
	}
}

void UStepModuleBPLibrary::RestoreStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->RestoreStep(InStep);
	}
}

void UStepModuleBPLibrary::EnterStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->EnterStep(InStep);
	}
}

void UStepModuleBPLibrary::RefreshStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->RefreshStep(InStep);
	}
}

void UStepModuleBPLibrary::GuideStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->GuideStep(InStep);
	}
}

void UStepModuleBPLibrary::ExecuteStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->ExecuteStep(InStep);
	}
}

void UStepModuleBPLibrary::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->CompleteStep(InStep, InStepExecuteResult);
	}
}

void UStepModuleBPLibrary::LeaveStep(UStepBase* InStep)
{
	if(AStepModule* StepModule = AMainModule::GetModuleByClass<AStepModule>())
	{
		StepModule->LeaveStep(InStep);
	}
}
