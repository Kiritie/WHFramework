// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/RootProcedureBase.h"

#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

ARootProcedureBase::ARootProcedureBase()
{
	ProcedureName = FName("RootProcedureBase");
	ProcedureDisplayName = FText::FromString(TEXT("Root Procedure Base"));

	ProcedureType = EProcedureType::Root;

	ProcedureExecuteType = EProcedureExecuteType::Server;
}

#if WITH_EDITOR
void ARootProcedureBase::OnGenerate()
{
	Super::OnGenerate();
}

void ARootProcedureBase::OnUnGenerate()
{
	Super::OnUnGenerate();
}
#endif

void ARootProcedureBase::ServerOnInitialize_Implementation()
{
	Super::ServerOnInitialize_Implementation();
}

void ARootProcedureBase::ServerOnPrepare_Implementation()
{
	Super::ServerOnPrepare_Implementation();
}

void ARootProcedureBase::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	Super::ServerOnEnter_Implementation(InLastProcedure);
}

void ARootProcedureBase::ServerOnRefresh_Implementation(float DeltaSeconds)
{
	Super::ServerOnRefresh_Implementation(DeltaSeconds);
}

void ARootProcedureBase::ServerOnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	Super::ServerOnComplete_Implementation(InProcedureExecuteResult);
}

void ARootProcedureBase::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	Super::ServerOnLeave_Implementation(InNextProcedure);
}

void ARootProcedureBase::ServerOnClear_Implementation()
{
	Super::ServerOnClear_Implementation();
}
