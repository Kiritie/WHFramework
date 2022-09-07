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

void UDebugModuleBPLibrary::DebugMessage(const FString& Message, const FColor& DisplayColor, float Duration, EDebugMode DebugMode, int32 Key, bool bNewerOnTop)
{
	switch (DebugMode)
	{
		case EDebugMode::All:
		{
			GEngine->AddOnScreenDebugMessage(Key, Duration, DisplayColor, Message, bNewerOnTop);
			WHLog(WH_Debug, Log, TEXT("%s"), *Message);
			break;
		}
		case EDebugMode::Screen:
		{
			GEngine->AddOnScreenDebugMessage(Key, Duration, DisplayColor, Message, bNewerOnTop);
			break;
		}
		case EDebugMode::Console:
		{
			WHLog(WH_Debug, Log, TEXT("%s"), *Message);
			break;
		}
	}
}
