// Copyright Epic Games, Inc. All Rights Reserved.


#include "WHFrameworkSlate.h"

#include "WHFrameworkSlateStyle.h"

void FWHFrameworkSlateModule::StartupModule()
{
	FWHFrameworkSlateStyle::ResetToDefault();
}

void FWHFrameworkSlateModule::ShutdownModule()
{
	
}

IMPLEMENT_MODULE(FWHFrameworkSlateModule, WHFrameworkSlate)
