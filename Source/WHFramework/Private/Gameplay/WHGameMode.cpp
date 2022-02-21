// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameMode.h"

#include "Main/MainModule.h"

AWHGameMode::AWHGameMode()
{
	
}

void AWHGameMode::BeginPlay()
{
	Super::BeginPlay();

	if(AMainModule* MainModule = AMainModule::Get())
	{
		MainModule->InitializeModules();
	}
}
