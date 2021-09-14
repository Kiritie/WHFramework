// Copyright 2015-2017 IVREAL, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugModuleTypes.h"

#include "Engine/World.h"

int32 EStatusLevelToInt(EStatusLevel StatusLevel)
{
	switch (StatusLevel)
    {
    	case EStatusLevel::ERF_Log:
    		return 1;
    	case EStatusLevel::ERF_Warning:
    		return 2;
    	case EStatusLevel::ERF_Error:
    		return 3;
		default :
			return 1;
    }
}

void FWHLog::StatusLog(int32 device_status_code, EStatusLevel device_status_level, const FString& device_status_info)
{
	if (GWorld)
	{
		
	}
}
