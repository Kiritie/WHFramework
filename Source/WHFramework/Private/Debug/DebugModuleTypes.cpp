// Copyright 2015-2017 WHFramework, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugModuleTypes.h"

void WHDebug(const FString& Message, FColor DisplayColor, float Duration, bool bNewerOnTop)
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, DisplayColor, Message, bNewerOnTop);
	}
	UE_LOG(WH_Debug, Log, TEXT("%s"), *Message);
}
