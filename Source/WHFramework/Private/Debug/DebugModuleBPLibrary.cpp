// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

void UDebugModuleBPLibrary::EnsureCrash(const FString& Message, bool bNoCrash)
{
	ensureMsgf(bNoCrash, TEXT("%s"), *Message);
}

void UDebugModuleBPLibrary::EnsureEditorCrash(const FString& Message, bool bNoCrash)
{
#if WITH_EDITOR
	ensureEditorMsgf(bNoCrash, TEXT("%s"), *Message);
#else
	if(!bNoCrash)
	{
		WHLog(LogTemp, Error, TEXT("Ensure Editor Crash : %s"), *Message);
	}
#endif
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
