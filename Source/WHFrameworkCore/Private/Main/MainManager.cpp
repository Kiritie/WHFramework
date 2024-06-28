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

#if !IS_PROGRAM
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FMainManager::OnWorldAdded);
#endif
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

void FMainManager::OnWorldAdded(UWorld* InWorld)
{
	if (InWorld && InWorld->WorldType != EWorldType::Editor)
	{
		if(!GWorldContext)
		{
			GWorldContext = GWorld;
		}
		OnPreparatory();
	}
}

void FMainManager::Tick(float DeltaSeconds)
{
	OnRefresh(DeltaSeconds);
}

TStatId FMainManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(AMainModule, STATGROUP_Tickables);
}
