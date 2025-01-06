// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainManager.h"

const FUniqueType FMainManager::Type = FUniqueType(&FManagerBase::Type);

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

#if WITH_ENGINE
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FMainManager::OnWorldAdded);
#endif

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime)
	{
		OnRefresh(DeltaTime);
		return true;
	}));
}

void FMainManager::OnPreparatory()
{
	FManagerBase::OnPreparatory();

	for(const auto Iter : ManagerMap)
	{
		Iter.Value->OnPreparatory();
	}
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

#if WITH_ENGINE
void FMainManager::OnWorldAdded(UWorld* InWorld)
{
	if (InWorld && InWorld->WorldType != EWorldType::Editor)
	{
		if(!GWorldContext)
		{
			GWorldContext = InWorld;
		}
		OnPreparatory();
	}
}
#endif