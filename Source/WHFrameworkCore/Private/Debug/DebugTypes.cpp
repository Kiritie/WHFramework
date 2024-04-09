// Copyright 2015-2017 WHFramework, Inc. All Rights Reserved.

#include "Debug/DebugTypes.h"

#include "Debug/DebugManager.h"

void WHLog(const FString& Message, EDebugCategory Category, EDebugVerbosity Verbosity)
{
	FDebugManager::Get().LogMessage(Message, Category, Verbosity);
}

void WHDebug(const FString& Message, EDebugMode Mode, EDebugCategory Category, EDebugVerbosity Verbosity, const FColor& DisplayColor, float Duration, int32 Key, bool bNewerOnTop)
{
	FDebugManager::Get().DebugMessage(Message, Mode, Category, Verbosity, DisplayColor, Duration, Key, bNewerOnTop);
}
