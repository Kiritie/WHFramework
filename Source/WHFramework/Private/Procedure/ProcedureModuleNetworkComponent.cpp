// Fill out your copyright notice in the Description page of Project Settings.

#include "Procedure/ProcedureModuleNetworkComponent.h"

#include "Main/MainModule.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"

UProcedureModuleNetworkComponent::UProcedureModuleNetworkComponent()
{
	
}

bool UProcedureModuleNetworkComponent::ServerEnterProcedure_Validate(AProcedureBase* InProcedure) { return true; }
void UProcedureModuleNetworkComponent::ServerEnterProcedure_Implementation(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerEnterProcedure(InProcedure);
	}
}

bool UProcedureModuleNetworkComponent::ServerLeaveProcedure_Validate(AProcedureBase* InProcedure) { return true; }
void UProcedureModuleNetworkComponent::ServerLeaveProcedure_Implementation(AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerLeaveProcedure(InProcedure);
	}
}

bool UProcedureModuleNetworkComponent::ServerSkipProcedure_Validate(class AProcedureBase* InProcedure) { return true; }
void UProcedureModuleNetworkComponent::ServerSkipProcedure_Implementation(class AProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ServerSkipProcedure(InProcedure);
	}
}
