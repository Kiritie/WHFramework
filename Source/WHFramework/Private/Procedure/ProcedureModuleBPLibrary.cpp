// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Procedure/Base/ProcedureBase.h"

UProcedureBase* UProcedureModuleBPLibrary::GetCurrentProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentProcedure();
	}
	return nullptr;
}

URootProcedureBase* UProcedureModuleBPLibrary::GetCurrentRootProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentRootProcedure();
	}
	return nullptr;
}

void UProcedureModuleBPLibrary::StartProcedure(int32 InRootProcedureIndex, bool bSkipProcedures)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->StartProcedure(InRootProcedureIndex, bSkipProcedures);
	}
}

void UProcedureModuleBPLibrary::EndProcedure(bool bRestoreProcedures)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->EndProcedure(bRestoreProcedures);
	}
}

void UProcedureModuleBPLibrary::RestoreProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RestoreProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::EnterProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->EnterProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::RefreshProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RefreshProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::GuideProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->GuideProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::ExecuteProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ExecuteProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->CompleteProcedure(InProcedure, InProcedureExecuteResult);
	}
}

void UProcedureModuleBPLibrary::LeaveProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->LeaveProcedure(InProcedure);
	}
}
