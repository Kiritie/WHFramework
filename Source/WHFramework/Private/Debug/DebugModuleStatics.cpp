// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleStatics.h"

#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

bool UDebugModuleStatics::EnsureEditor(bool Expression)
{
	return ensureEditor(Expression);
}

bool UDebugModuleStatics::EnsureEditorMsgf(bool Expression, const FString& Message, EDebugCategory Category, EDebugVerbosity Verbosity)
{
	return ensureEditorMsgf(Expression, Message, Category, Verbosity);
}

void UDebugModuleStatics::LogMessage(const FString& Message, EDebugCategory Category, EDebugVerbosity Verbosity)
{
	if(!GetDebugCategoryState(Category).bEnableConsoleLog) return;

	#define LOG_CASE(Category, Verbosity) \
	case EDC_##Category: \
	{ \
		switch (Verbosity) \
		{ \
			case EDV_Log: UE_LOG(WH_##Category, Log, TEXT("%s"), *Message); break; \
			case EDV_Warning: UE_LOG(WH_##Category, Warning, TEXT("%s"), *Message); break; \
			case EDV_Error: UE_LOG(WH_##Category, Error, TEXT("%s"), *Message); break; \
		} \
		break; \
	}

	switch(Category)
	{
		LOG_CASE(Default, Verbosity)
		LOG_CASE(Editor, Verbosity)
		LOG_CASE(Controller, Verbosity)
		LOG_CASE(Ability, Verbosity)
		LOG_CASE(Achievement, Verbosity)
		LOG_CASE(Asset, Verbosity)
		LOG_CASE(Audio, Verbosity)
		LOG_CASE(Camera, Verbosity)
		LOG_CASE(Character, Verbosity)
		LOG_CASE(Event, Verbosity)
		LOG_CASE(FSM, Verbosity)
		LOG_CASE(Input, Verbosity)
		LOG_CASE(LatentAction, Verbosity)
		LOG_CASE(Media, Verbosity)
		LOG_CASE(Network, Verbosity)
		LOG_CASE(ObjectPool, Verbosity)
		LOG_CASE(Parameter, Verbosity)
		LOG_CASE(Procedure, Verbosity)
		LOG_CASE(ReferencePool, Verbosity)
		LOG_CASE(SaveGame, Verbosity)
		LOG_CASE(Scene, Verbosity)
		LOG_CASE(Step, Verbosity)
		LOG_CASE(Task, Verbosity)
		LOG_CASE(Voxel, Verbosity)
		LOG_CASE(WebRequest, Verbosity)
		LOG_CASE(Widget, Verbosity)
	}
}

void UDebugModuleStatics::DebugMessage(const FString& Message, EDebugMode Mode, EDebugCategory Category, EDebugVerbosity Verbosity, const FColor& DisplayColor, float Duration, int32 Key, bool bNewerOnTop)
{
	const auto State = GetDebugCategoryState(Category);
	switch (Mode)
	{
		case EDM_All:
		{
			if(State.bEnableScreenLog)
			{
				GEngine->AddOnScreenDebugMessage(Key, Duration, DisplayColor, Message, bNewerOnTop);
			}
			if(State.bEnableConsoleLog)
			{
				LogMessage(Message, Category, Verbosity);
			}
			break;
		}
		case EDM_Screen:
		{
			if(State.bEnableScreenLog)
			{
				GEngine->AddOnScreenDebugMessage(Key, Duration, DisplayColor, Message, bNewerOnTop);
			}
			break;
		}
		case EDM_Console:
		{
			if(State.bEnableConsoleLog)
			{
				LogMessage(Message, Category, Verbosity);
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
