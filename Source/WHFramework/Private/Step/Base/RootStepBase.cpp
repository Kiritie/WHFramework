// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Base/RootStepBase.h"

#include "Step/StepModule.h"
#include "Step/StepModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

URootStepBase::URootStepBase()
{
	StepName = FName("RootStepBase");
	StepDisplayName = FText::FromString(TEXT("Root Step Base"));

	StepType = EStepType::Root;
}

#if WITH_EDITOR
void URootStepBase::OnGenerate()
{
	Super::OnGenerate();
}

void URootStepBase::OnUnGenerate()
{
	Super::OnUnGenerate();
}
#endif

void URootStepBase::OnInitialize()
{
	Super::OnInitialize();
}

void URootStepBase::OnRestore()
{
	Super::OnRestore();
}

void URootStepBase::OnEnter(UStepBase* InLastStep)
{
	Super::OnEnter(InLastStep);
}

void URootStepBase::OnRefresh()
{
	Super::OnRefresh();
}

void URootStepBase::OnGuide()
{
	Super::OnGuide();
}

void URootStepBase::OnExecute()
{
	Super::OnExecute();
}

void URootStepBase::OnComplete(EStepExecuteResult InStepExecuteResult)
{
	Super::OnComplete(InStepExecuteResult);
}

void URootStepBase::OnLeave()
{
	Super::OnLeave();
}
