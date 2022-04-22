// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/RootProcedureBase.h"

#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

URootProcedureBase::URootProcedureBase()
{
	ProcedureName = FName("RootProcedureBase");
	ProcedureDisplayName = FText::FromString(TEXT("Root Procedure Base"));

	ProcedureType = EProcedureType::Root;
}

#if WITH_EDITOR
void URootProcedureBase::OnGenerate()
{
	Super::OnGenerate();
}

void URootProcedureBase::OnUnGenerate()
{
	Super::OnUnGenerate();
}
#endif

void URootProcedureBase::OnInitialize()
{
	Super::OnInitialize();
}

void URootProcedureBase::OnRestore()
{
	Super::OnRestore();
}

void URootProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	Super::OnEnter(InLastProcedure);
}

void URootProcedureBase::OnRefresh()
{
	Super::OnRefresh();
}

void URootProcedureBase::OnGuide()
{
	Super::OnGuide();
}

void URootProcedureBase::OnExecute()
{
	Super::OnExecute();
}

void URootProcedureBase::OnComplete(EProcedureExecuteResult InProcedureExecuteResult)
{
	Super::OnComplete(InProcedureExecuteResult);
}

void URootProcedureBase::OnLeave()
{
	Super::OnLeave();
}
