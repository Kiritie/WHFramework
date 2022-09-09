// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Procedure/Base/ProcedureBase.h"

UProcedureBase* UProcedureModuleBPLibrary::GetCurrentProcedure(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetCurrentProcedure();
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::HasProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByIndex(InProcedureIndex);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByIndex(InProcedureIndex, InProcedureClass);
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByClass(InProcedureClass);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByClass(InProcedureClass);
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::IsCurrentProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->IsCurrentProcedure(InProcedure);
	}
	return false;
}

bool UProcedureModuleBPLibrary::IsCurrentProcedureIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->IsCurrentProcedureIndex(InProcedureIndex);
	}
	return false;
}

void UProcedureModuleBPLibrary::SwitchProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::SwitchProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::SwitchFirstProcedure()
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchFirstProcedure();
	}
}

void UProcedureModuleBPLibrary::SwitchLastProcedure()
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchLastProcedure();
	}
}

void UProcedureModuleBPLibrary::SwitchNextProcedure()
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchNextProcedure();
	}
}

void UProcedureModuleBPLibrary::GuideCurrentProcedure()
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->GuideCurrentProcedure();
	}
}
