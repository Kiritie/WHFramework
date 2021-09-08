// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "WidgetModule.h"
#include "Kismet/GameplayStatics.h"

AWidgetModule* UWidgetModuleBPLibrary::WidgetModule = nullptr;

AWidgetModule* UWidgetModuleBPLibrary::GetWidgetModule(UObject* InWorldContext)
{
	if ((!WidgetModule || !WidgetModule->IsValidLowLevel()) && (InWorldContext && InWorldContext->IsValidLowLevel()))
	{
		WidgetModule = Cast<AWidgetModule>(UGameplayStatics::GetActorOfClass(InWorldContext, AWidgetModule::StaticClass()));
	}
	return WidgetModule;
}
