// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
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

bool UProcedureModuleBPLibrary::HasProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->HasProcedureByIndex(InProcedureIndex);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::K2_GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->K2_GetProcedureByIndex(InProcedureIndex, InProcedureClass);
	}
	return nullptr;
}

bool UProcedureModuleBPLibrary::K2_HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->K2_HasProcedureByClass(InProcedureClass);
	}
	return false;
}

UProcedureBase* UProcedureModuleBPLibrary::K2_GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		return ProcedureModule->K2_GetProcedureByClass(InProcedureClass);
	}
	return nullptr;
}

void UProcedureModuleBPLibrary::StartProcedure(int32 InProcedureIndex, bool bSkipProcedures)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->StartProcedure(InProcedureIndex, bSkipProcedures);
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

void UProcedureModuleBPLibrary::RestoreProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RestoreProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::RestoreProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RestoreProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::EnterProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->EnterProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::EnterProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->EnterProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::EnterProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->EnterProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::RefreshProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RefreshProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::RefreshProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RefreshProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::RefreshProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->RefreshProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::GuideProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->GuideProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::GuideProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->GuideProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::GuideProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->GuideProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::ExecuteProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ExecuteProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::ExecuteProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ExecuteProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::ExecuteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->ExecuteProcedureByClass(InProcedureClass);
	}
}

void UProcedureModuleBPLibrary::CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->CompleteProcedure(InProcedure, InProcedureExecuteResult);
	}
}

void UProcedureModuleBPLibrary::CompleteProcedureByIndex(int32 InProcedureIndex, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->CompleteProcedureByIndex(InProcedureIndex, InProcedureExecuteResult);
	}
}

void UProcedureModuleBPLibrary::CompleteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass, EProcedureExecuteResult InProcedureExecuteResult)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->CompleteProcedureByClass(InProcedureClass, InProcedureExecuteResult);
	}
}

void UProcedureModuleBPLibrary::LeaveProcedure(UProcedureBase* InProcedure)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->LeaveProcedure(InProcedure);
	}
}

void UProcedureModuleBPLibrary::LeaveProcedureByIndex(int32 InProcedureIndex)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->LeaveProcedureByIndex(InProcedureIndex);
	}
}

void UProcedureModuleBPLibrary::LeaveProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
	{
		ProcedureModule->LeaveProcedureByClass(InProcedureClass);
	}
}
