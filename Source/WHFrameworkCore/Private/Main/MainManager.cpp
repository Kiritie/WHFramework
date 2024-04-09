// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainManager.h"

const FUniqueType FMainManager::Type = FUniqueType();

IMPLEMENTATION_MAIN_MANAGER(FMainManager)

// ParamSets default values
FMainManager::FMainManager()
{
	ManagerMap = TMap<FUniqueType, FManagerBase*>();
}

FMainManager::~FMainManager()
{
}

void FMainManager::OnInitialize()
{
	FManagerBase::OnInitialize();
}
