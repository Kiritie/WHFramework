// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleBPLibrary.h"

#include "Procedure/Base/ProcedureBase.h"

UProcedureBase* UProcedureModuleBPLibrary::GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetCurrentProcedure();
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::HasProcedureByIndex(int32 InIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByIndex(InIndex);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByIndex(InIndex, InClass);
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByClass(InClass);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByClass(InClass);
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

bool UProcedureModuleBPLibrary::IsCurrentProcedureIndex(int32 InIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		return ProcedureModule->IsCurrentProcedureIndex(InIndex);
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

void UProcedureModuleBPLibrary::SwitchProcedureByIndex(int32 InIndex)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByIndex(InIndex);
	}
}

void UProcedureModuleBPLibrary::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByClass(InClass);
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
