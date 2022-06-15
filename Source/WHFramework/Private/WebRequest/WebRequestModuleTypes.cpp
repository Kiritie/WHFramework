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
		if(ContentMap.GetNum() > 0)
		{
			FString ContentStr;
			switch(ContentType)
			{
				case EWebContentType::Form:
				{
					for(auto& Iter : ContentMap.GetMap())
					{
						ContentStr.Append(FString::Printf(TEXT("%s=%s&"), *Iter.Key, *Iter.Value));
					}
					ContentStr.RemoveFromEnd(TEXT("&"));
					break;
				}
				case EWebContentType::Text:
				{
					ContentStr = ContentMap.ToString();
					break;
				}
				case EWebContentType::Json:
				{
					ContentStr = ContentMap.ToJsonString();
					break;
				}
			}
			return ContentStr;
		}
	}
	return TEXT("");
}
