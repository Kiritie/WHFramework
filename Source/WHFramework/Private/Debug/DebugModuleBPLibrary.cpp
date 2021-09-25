// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Debug/DebugModule.h"

ADebugModule* UDebugModuleBPLibrary::DebugModuleInst = nullptr;

ADebugModule* UDebugModuleBPLibrary::GetDebugModule(UObject* InWorldContext)
{
	if (!DebugModuleInst || !DebugModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			DebugModuleInst = MainModule->GetModuleByClass<ADebugModule>();
		}
	}
	return DebugModuleInst;
}
