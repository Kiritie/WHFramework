// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleStatics.h"

#include "Procedure/Base/ProcedureBase.h"

UProcedureBase* UProcedureModuleStatics::GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->GetCurrentProcedure();
	}
	return nullptr;
}

bool UProcedureModuleStatics::HasProcedureByIndex(int32 InIndex)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByIndex(InIndex);
	}
	return false;
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByIndex(InIndex, InClass);
	}
	return nullptr;
}

bool UProcedureModuleStatics::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->HasProcedureByClass(InClass);
	}
	return false;
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->GetProcedureByClass(InClass);
	}
	return nullptr;
}

bool UProcedureModuleStatics::IsCurrentProcedure(UProcedureBase* InProcedure)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->IsCurrentProcedure(InProcedure);
	}
	return false;
}

bool UProcedureModuleStatics::IsCurrentProcedureIndex(int32 InIndex)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		return ProcedureModule->IsCurrentProcedureIndex(InIndex);
	}
	return false;
}

void UProcedureModuleStatics::SwitchProcedure(UProcedureBase* InProcedure)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedure(InProcedure);
	}
}

void UProcedureModuleStatics::SwitchProcedureByIndex(int32 InIndex)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByIndex(InIndex);
	}
}

void UProcedureModuleStatics::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchProcedureByClass(InClass);
	}
}

void UProcedureModuleStatics::SwitchFirstProcedure()
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchFirstProcedure();
	}
}

void UProcedureModuleStatics::SwitchLastProcedure()
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchLastProcedure();
	}
}

void UProcedureModuleStatics::SwitchNextProcedure()
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->SwitchNextProcedure();
	}
}

void UProcedureModuleStatics::GuideCurrentProcedure()
{
	if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
	{
		ProcedureModule->GuideCurrentProcedure();
	}
}
