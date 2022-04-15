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

void URootProcedureBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void URootProcedureBase::OnRestore_Implementation()
{
	Super::OnRestore_Implementation();
}

void URootProcedureBase::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	Super::OnEnter_Implementation(InLastProcedure);
}

void URootProcedureBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void URootProcedureBase::OnGuide_Implementation()
{
	Super::OnGuide_Implementation();
}

void URootProcedureBase::OnExecute_Implementation()
{
	Super::OnExecute_Implementation();
}

void URootProcedureBase::OnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	Super::OnComplete_Implementation(InProcedureExecuteResult);
}

void URootProcedureBase::OnLeave_Implementation()
{
	Super::OnLeave_Implementation();
}
