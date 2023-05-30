// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Base/RootTaskBase.h"

#include "Task/TaskModule.h"
#include "Task/TaskModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

URootTaskBase::URootTaskBase()
{
	TaskDisplayName = FText::FromString(TEXT("Root Task Base"));

	TaskType = ETaskType::Root;
}

#if WITH_EDITOR
void URootTaskBase::OnGenerate()
{
	Super::OnGenerate();
}

void URootTaskBase::OnUnGenerate()
{
	Super::OnUnGenerate();
}
#endif

void URootTaskBase::OnInitialize()
{
	Super::OnInitialize();
}

void URootTaskBase::OnRestore()
{
	Super::OnRestore();
}

void URootTaskBase::OnEnter(UTaskBase* InLastTask)
{
	Super::OnEnter(InLastTask);
}

void URootTaskBase::OnRefresh()
{
	Super::OnRefresh();
}

void URootTaskBase::OnGuide()
{
	Super::OnGuide();
}

void URootTaskBase::OnExecute()
{
	Super::OnExecute();
}

void URootTaskBase::OnComplete(ETaskExecuteResult InTaskExecuteResult)
{
	Super::OnComplete(InTaskExecuteResult);
}

void URootTaskBase::OnLeave()
{
	Super::OnLeave();
}
