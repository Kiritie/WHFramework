// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

AWebRequestModule* UWebRequestModuleBPLibrary::GetWebRequestModule(UObject* InWorldContext)
{
	if (!WebRequestModuleInst || !WebRequestModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			WebRequestModuleInst = MainModule->GetModuleByClass<AWebRequestModule>();
		}
	}
	return WebRequestModuleInst;
}
