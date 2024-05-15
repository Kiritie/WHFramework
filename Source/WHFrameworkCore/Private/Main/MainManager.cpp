// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainManager.h"

const FUniqueType FMainManager::Type = FUniqueType();

IMPLEMENTATION_MAIN_MANAGER(FMainManager)

// ParamSets default values
FMainManager::FMainManager() : FManagerBase(Type)
{
	ManagerMap = TMap<FUniqueType, FManagerBase*>();
}

FMainManager::~FMainManager()
{
}

void FMainManager::OnInitialize()
{
	FManagerBase::OnInitialize();

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaSeconds)
	{
		OnRefresh(DeltaSeconds);
		return true;
	}));
}

void FMainManager::OnRefresh(float DeltaSeconds)
{
	FManagerBase::OnRefresh(DeltaSeconds);

	for(const auto Iter : ManagerMap)
	{
		Iter.Value->OnRefresh(DeltaSeconds);
	}
}

void FMainManager::OnTermination()
{
	FManagerBase::OnTermination();
}
