// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Procedure/Base/ProcedureBase.h"


AProcedureBase* UProcedureModuleBPLibrary::GetCurrentProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentProcedure();
	}
	return nullptr;
}

AProcedureBase* UProcedureModuleBPLibrary::GetCurrentLocalProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentLocalProcedure();
	}
	return nullptr;
}

AProcedureBase* UProcedureModuleBPLibrary::GetCurrentParentProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentParentProcedure();
	}
	return nullptr;
}

ARootProcedureBase* UProcedureModuleBPLibrary::GetCurrentRootProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->GetCurrentRootProcedure();
	}
	return nullptr;
}

void UProcedureModuleBPLibrary::ServerEnterProcedure(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerEnterProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::ServerLeaveProcedure(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerLeaveProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::LocalEnterProcedure(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerEnterProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::LocalLeaveProcedure(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->LocalLeaveProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::ServerSkipCurrentProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerSkipCurrentProcedure();
	}
}

void UProcedureModuleBPLibrary::ServerSkipCurrentParentProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerSkipCurrentParentProcedure();
	}
}

void UProcedureModuleBPLibrary::ServerSkipCurrentRootProcedure()
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerSkipCurrentRootProcedure();
	}
}
