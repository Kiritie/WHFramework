// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHWorldSubsystem.h"

#include "EngineUtils.h"
#include "Gameplay/WHGameManager.h"
#include "Gameplay/WHGameMode.h"
#include "Main/MainModule.h"

UWHWorldSubsystem::UWHWorldSubsystem()
{
	
}

void UWHWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	const FScopedWorldContext WorldContextScope(&InWorld);
	Super::OnWorldBeginPlay(InWorld);

	if(AWHGameMode* GameMode = InWorld.GetAuthGameMode<AWHGameMode>())
	{
		if(!IWHActorInterface::Execute_IsInitialized(GameMode))
		{
			GameMode->Execute_OnInitialize(GameMode);
		}
	}

	AMainModule* MainModule = AMainModule::GetPtr();
	if(MainModule)
	{
		if(!IWHActorInterface::Execute_IsInitialized(MainModule))
		{
			IWHActorInterface::Execute_OnInitialize(MainModule);
		}
	}

	for(TActorIterator<AWHGameManager> Iter(&InWorld); Iter; ++Iter)
	{
		AWHGameManager* GameManager = *Iter;
		if(!IWHActorInterface::Execute_IsInitialized(GameManager))
		{
			IWHActorInterface::Execute_OnInitialize(GameManager);
		}
	}

	for(TActorIterator<AActor> Iter(&InWorld); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if(Actor->Implements<UWHActorInterface>() && !IWHActorInterface::Execute_IsInitialized(Actor))
		{
			IWHActorInterface::Execute_OnInitialize(Actor);
		}
	}

	// 全部 Actor 初始化后，由每个 World 统一准备模块；客户端同样需要完整阶段。
	if (MainModule)
	{
		IWHActorInterface::Execute_OnPreparatory(MainModule);
	}
}
