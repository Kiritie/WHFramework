// Copyright Epic Games, Inc. All Rights Reserved.

#include "Application/ApplicationManager.h"

#include "Main/MainManager.h"

FUniqueType FApplicationManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FApplicationManager)

FApplicationManager::FApplicationManager() : FManagerBase(Type)
{
}

void FApplicationManager::OnInitialize()
{
	FManagerBase::OnInitialize();
}
