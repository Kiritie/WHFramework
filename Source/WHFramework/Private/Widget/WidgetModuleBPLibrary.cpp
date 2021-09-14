// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "MainModule.h"
#include "MainModuleBPLibrary.h"
#include "WidgetModule.h"
#include "Kismet/GameplayStatics.h"

AWidgetModule* UWidgetModuleBPLibrary::WidgetModuleInst = nullptr;

AWidgetModule* UWidgetModuleBPLibrary::GetWidgetModule(UObject* InWorldContext)
{
	if (!WidgetModuleInst || !WidgetModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			WidgetModuleInst = MainModule->GetModuleByClass<AWidgetModule>();
		}
	}
	return WidgetModuleInst;
}
