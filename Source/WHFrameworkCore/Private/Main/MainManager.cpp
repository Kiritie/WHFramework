// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainManager.h"
#include "Type/NativeTypeIdentity.h"

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

	WorldAddedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FMainManager::OnWorldAdded);

	RefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime)
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
	if (bTerminating) return;
	bTerminating = true;
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(WorldAddedHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(RefreshHandle);
	WorldAddedHandle.Reset();
	RefreshHandle.Reset();
	TArray<FManagerBase*> Remaining;
	ManagerMap.GenerateValueArray(Remaining);
	ManagerMap.Empty();
	for (FManagerBase* Manager : Remaining)
	{
		if (Manager)
		{
			Manager->OnTermination();
			delete Manager;
		}
	}
	FManagerBase::OnTermination();
}

void FMainManager::Shutdown()
{
	if (Instance && !Instance->bTerminating)
	{
		FMainManager* Manager = Instance;
		Manager->OnTermination();
		Instance = nullptr;
		delete Manager;
	}
}

void FMainManager::Startup()
{
	if (!Instance)
	{
		Instance = new FMainManager();
		Instance->OnInitialize();
	}
}

void FMainManager::OnWorldAdded(UWorld* InWorld)
{
	if (InWorld && InWorld->WorldType != EWorldType::Editor)
	{
		OnPreparatory();
	}
}
