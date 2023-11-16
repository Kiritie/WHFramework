// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleStatics.h"

#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

bool UDebugModuleStatics::EnsureEditor(bool InExpression)
{
	return ensureEditor(InExpression);
}

bool UDebugModuleStatics::EnsureEditorMsgf(bool InExpression, const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity)
{
	return ensureEditorMsgf(InExpression, InMessage, InCategory, InVerbosity);
}

void UDebugModuleStatics::LogMessage(const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity)
{
	if(!GetDebugCategoryState(InCategory).bEnableConsoleLog) return;

	#define LOG_CASE(Category, Verbosity) \
	case EDC_##Category: \
	{ \
		switch (Verbosity) \
		{ \
			case EDV_Log: UE_LOG(WH_##Category, Log, TEXT("%s"), *InMessage); break; \
			case EDV_Warning: UE_LOG(WH_##Category, Warning, TEXT("%s"), *InMessage); break; \
			case EDV_Error: UE_LOG(WH_##Category, Error, TEXT("%s"), *InMessage); break; \
		} \
		break; \
	}

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
		LOG_CASE(LatentAction, InVerbosity)
		LOG_CASE(Media, InVerbosity)
		LOG_CASE(Network, InVerbosity)
		LOG_CASE(ObjectPool, InVerbosity)
		LOG_CASE(Parameter, InVerbosity)
		LOG_CASE(Procedure, InVerbosity)
		LOG_CASE(ReferencePool, InVerbosity)
		LOG_CASE(SaveGame, InVerbosity)
		LOG_CASE(Scene, InVerbosity)
		LOG_CASE(Step, InVerbosity)
		LOG_CASE(Task, InVerbosity)
		LOG_CASE(Voxel, InVerbosity)
		LOG_CASE(WebRequest, InVerbosity)
		LOG_CASE(Widget, InVerbosity)
	}
}

void UDebugModuleStatics::DebugMessage(const FString& InMessage, EDebugMode InMode, EDebugCategory InCategory, EDebugVerbosity InVerbosity, const FLinearColor InDisplayColor, float InDuration, int32 InKey, bool bNewerOnTop)
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

FDebugCategoryState UDebugModuleStatics::GetDebugCategoryState(EDebugCategory InCategory)
{
	return UDebugModule::Get().GetDebugCategoryState(InCategory);
}

void UDebugModuleStatics::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	UDebugModule::Get().SetDebugCategoryState(InCategory, InState);
}
