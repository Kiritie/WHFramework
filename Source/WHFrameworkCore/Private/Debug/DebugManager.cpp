// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugManager.h"

#include "WHFrameworkCoreTypes.h"
#include "Main/MainManager.h"

const FUniqueType FDebugManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FDebugManager)

// Sets default values
FDebugManager::FDebugManager() : FManagerBase(Type)
{
	DebugCategoryStates = TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState>();
	DON_WITHINDEX((int32)EDC_Custom1, i, 
		DebugCategoryStates.Add((EDebugCategory)i, FDebugCategoryState(true, true));
	)
}

FDebugManager::~FDebugManager()
{
}

void FDebugManager::LogMessage(const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity)
{
	if(!GetDebugCategoryState(InCategory).bEnableConsoleLog) return;

	switch(InCategory)
	{
		LOG_CASE(Default, InVerbosity)
		LOG_CASE(Editor, InVerbosity)
		LOG_CASE(Controller, InVerbosity)
		LOG_CASE(Ability, InVerbosity)
		LOG_CASE(Achievement, InVerbosity)
		LOG_CASE(Asset, InVerbosity)
		LOG_CASE(Audio, InVerbosity)
		LOG_CASE(Camera, InVerbosity)
		LOG_CASE(Character, InVerbosity)
		LOG_CASE(Event, InVerbosity)
		LOG_CASE(FSM, InVerbosity)
		LOG_CASE(Input, InVerbosity)
		LOG_CASE(Animation, InVerbosity)
		LOG_CASE(Media, InVerbosity)
		LOG_CASE(Network, InVerbosity)
		LOG_CASE(ObjectPool, InVerbosity)
		LOG_CASE(Parameter, InVerbosity)
		LOG_CASE(Pawn, InVerbosity)
		LOG_CASE(Procedure, InVerbosity)
		LOG_CASE(ReferencePool, InVerbosity)
		LOG_CASE(SaveGame, InVerbosity)
		LOG_CASE(Scene, InVerbosity)
		LOG_CASE(Step, InVerbosity)
		LOG_CASE(Task, InVerbosity)
		LOG_CASE(Voxel, InVerbosity)
		LOG_CASE(WebRequest, InVerbosity)
		LOG_CASE(Widget, InVerbosity)
		default: break;
	}
}

void FDebugManager::DebugMessage(const FString& InMessage, EDebugMode InMode, EDebugCategory InCategory, EDebugVerbosity InVerbosity, const FLinearColor InDisplayColor, float InDuration, int32 InKey, bool bNewerOnTop)
{
	const auto State = GetDebugCategoryState(InCategory);
	switch (InMode)
	{
		case EDM_All:
		{
			if(State.bEnableScreenLog)
			{
				GEngine->AddOnScreenDebugMessage(InKey, InDuration, InDisplayColor.ToFColor(false), InMessage, bNewerOnTop);
			}
			if(State.bEnableConsoleLog)
			{
				LogMessage(InMessage, InCategory, InVerbosity);
			}
			break;
		}
		case EDM_Screen:
		{
			if(State.bEnableScreenLog)
			{
				GEngine->AddOnScreenDebugMessage(InKey, InDuration, InDisplayColor.ToFColor(false), InMessage, bNewerOnTop);
			}
			break;
		}
		case EDM_Console:
		{
			if(State.bEnableConsoleLog)
			{
				LogMessage(InMessage, InCategory, InVerbosity);
			}
			break;
		}
	}
}

FDebugCategoryState FDebugManager::GetDebugCategoryState(EDebugCategory InCategory) const
{
	if(DebugCategoryStates.Contains(InCategory))
	{
		return DebugCategoryStates[InCategory];
	}
	return FDebugCategoryState();
}

void FDebugManager::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	DebugCategoryStates.Emplace(InCategory, InState);
}
