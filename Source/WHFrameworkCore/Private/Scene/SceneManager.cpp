// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneManager.h"

#include "WHFrameworkCoreTypes.h"
#include "Main/MainManager.h"

const FUniqueType FSceneManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FSceneManager)

// Sets default values
FSceneManager::FSceneManager() : FManagerBase(Type)
{
}

FSceneManager::FSceneManager(FUniqueType InType) : FManagerBase(InType)
{
}

FSceneManager::~FSceneManager()
{
}
