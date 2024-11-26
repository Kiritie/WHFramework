// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/StepModuleStatics.h"

#include "Step/StepModule.h"

TArray<UStepAsset*> UStepModuleStatics::GetStepAssets()
{
	return UStepModule::Get().GetAssets();
}

UStepAsset* UStepModuleStatics::GetCurrentStepAsset()
{
	return UStepModule::Get().GetCurrentAsset();
}

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

UStepAsset* UStepModuleStatics::GetStepAsset(UStepAsset* InAsset)
{
	return UStepModule::Get().GetAsset(InAsset);
}

void UStepModuleStatics::AddStepAsset(UStepAsset* InAsset)
{
	UStepModule::Get().AddAsset(InAsset);
}

void UStepModuleStatics::RemoveStepAsset(UStepAsset* InAsset)
{
	UStepModule::Get().RemoveAsset(InAsset);
}

void UStepModuleStatics::SwitchStepAsset(UStepAsset* InAsset)
{
	UStepModule::Get().SwitchAsset(InAsset);
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
