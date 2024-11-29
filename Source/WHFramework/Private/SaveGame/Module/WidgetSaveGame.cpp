// Copyright 2021 Sam Carey. All Rights Reserved.

#include "SaveGame/Module/WidgetSaveGame.h"

#include "Widget/WidgetModule.h"

UWidgetSaveGame::UWidgetSaveGame()
{
	SaveName = FName("Widget");

	ModuleClass = UWidgetModule::StaticClass();
}

void UWidgetSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);
}

void UWidgetSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UWidgetSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UWidgetSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
}
