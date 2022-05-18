// Copyright 2015-2017 WHFramework, Inc. All Rights Reserved.

#pragma once

#include "WebRequest/WebRequestModuleTypes.h"

#include "WebRequest/WebRequestModuleBPLibrary.h"

FString FWebContent::ToString()
{
	if(!Content.IsEmpty())
	{
		return Content;
	}
	else
	{
		return UWebRequestModuleBPLibrary::ParseWebContentToString(ContentMap, ContentType);
	}
}
