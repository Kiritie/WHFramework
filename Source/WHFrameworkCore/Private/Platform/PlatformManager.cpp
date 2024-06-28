// Fill out your copyright notice in the Description page of Project Settings.

#include "Platform/PlatformManager.h"

#include "Main/MainManager.h"

const FUniqueType FPlatformManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FPlatformManager)

// Sets default values
FPlatformManager::FPlatformManager() : FManagerBase(Type)
{
}

FPlatformManager::~FPlatformManager()
{
}

void FPlatformManager::OnInitialize()
{
	FManagerBase::OnInitialize();
}

void FPlatformManager::OnTermination()
{
	FManagerBase::OnTermination();
}