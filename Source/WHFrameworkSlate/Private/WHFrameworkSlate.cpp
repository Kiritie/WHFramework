// Copyright Epic Games, Inc. All Rights Reserved.


#include "WHFrameworkSlate.h"

#include "WHFrameworkSlateStyle.h"
#include "Slate/SlateWidgetManager.h"

void FWHFrameworkSlateModule::StartupModule()
{
	FSlateWidgetManager::Register();

	FWHFrameworkSlateStyle::Initialize();
}

void FWHFrameworkSlateModule::ShutdownModule()
{
	FSlateWidgetManager::UnRegister();

	FWHFrameworkSlateStyle::Shutdown();
}

IMPLEMENT_MODULE(FWHFrameworkSlateModule, WHFrameworkSlate)
