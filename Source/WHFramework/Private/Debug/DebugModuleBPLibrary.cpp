// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

void UDebugModuleBPLibrary::Ensure(bool Expression)
{
	ensureAlways(Expression);
}

void UDebugModuleBPLibrary::EnsureEditor(bool Expression)
{
	ensureEditor(Expression);
}

void UDebugModuleBPLibrary::EnsureMsgf(const FString& Message, bool Expression)
{
	ensureMsgf(Expression, TEXT("%s"), *Message);
}

void UDebugModuleBPLibrary::EnsureEditorMsgf(const FString& Message, bool Expression)
{
	ensureEditorMsgf(Expression, TEXT("%s"), *Message);
}

void UDebugModuleBPLibrary::LogMessage(const FString& Message, EDebugCategory Category, EDebugVerbosity Verbosity)
{
	if(!GetDebugCategoryState(Category).bEnableConsoleLog) return;
		
	switch(Category)
	{
		case EDebugCategory::Default:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Default, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Default, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Default, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Editor:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Editor, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Editor, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Editor, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Controller:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Controller, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Controller, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Controller, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Ability:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Ability, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Ability, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Ability, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Asset:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Asset, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Asset, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Asset, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Audio:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Audio, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Audio, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Audio, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Camera:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Camera, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Camera, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Camera, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Character:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Character, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Character, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Character, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Event:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Event, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Event, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Event, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::FSM:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_FSM, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_FSM, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_FSM, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Input:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Input, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Input, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Input, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::LatentAction:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_LatentAction, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_LatentAction, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_LatentAction, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Media:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Media, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Media, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Media, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Network:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Network, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Network, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Network, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::ObjectPool:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_ObjectPool, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_ObjectPool, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_ObjectPool, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Parameter:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Parameter, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Parameter, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Parameter, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Procedure:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Procedure, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Procedure, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Procedure, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::ReferencePool:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_ReferencePool, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_ReferencePool, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_ReferencePool, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::SaveGame:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_SaveGame, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_SaveGame, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_SaveGame, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Scene:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Scene, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Scene, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Scene, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Step:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Step, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Step, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Step, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Task:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Task, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Task, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Task, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Voxel:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Voxel, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Voxel, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Voxel, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::WebRequest:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_WebRequest, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_WebRequest, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_WebRequest, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
		case EDebugCategory::Widget:
		{
			switch (Verbosity)
			{
				case EDebugVerbosity::Log: WH_LOG(WH_Widget, Log, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Warning: WH_LOG(WH_Widget, Warning, TEXT("%s"), *Message); break;
				case EDebugVerbosity::Error: WH_LOG(WH_Widget, Error, TEXT("%s"), *Message); break;
			}
			break;
		}
	}
}

void UDebugModuleBPLibrary::DebugMessage(const FString& Message, EDebugMode Mode, EDebugCategory Category, EDebugVerbosity Verbosity, const FColor& DisplayColor, float Duration, int32 Key, bool bNewerOnTop)
{
	const auto State = GetDebugCategoryState(Category);
	switch (Mode)
	{
		case EDebugMode::All:
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
		case EDebugMode::Screen:
		{
			if(State.bEnableScreenLog)
			{
				GEngine->AddOnScreenDebugMessage(Key, Duration, DisplayColor, Message, bNewerOnTop);
			}
			break;
		}
		case EDebugMode::Console:
		{
			if(State.bEnableConsoleLog)
			{
				LogMessage(Message, Category, Verbosity);
			}
			break;
		}
	}
}

FDebugCategoryState UDebugModuleBPLibrary::GetDebugCategoryState(EDebugCategory InCategory)
{
	if(ADebugModule::Get())
	{
		return ADebugModule::Get()->GetDebugCategoryState(InCategory);
	}
	return FDebugCategoryState();
}

void UDebugModuleBPLibrary::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	if(ADebugModule::Get())
	{
		ADebugModule::Get()->SetDebugCategoryState(InCategory, InState);
	}
}
