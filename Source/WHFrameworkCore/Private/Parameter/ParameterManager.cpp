// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterManager.h"

#include "Main/MainManager.h"

// Add default functionality here for any IParameterManager functions that are not pure virtual.

const FUniqueType FParameterManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FParameterManager)

FParameterManager::FParameterManager() : FManagerBase(Type)
{
}

FParameterManager::~FParameterManager()
{
}
