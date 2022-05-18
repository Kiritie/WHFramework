// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

FString UWebRequestModuleBPLibrary::ParseWebContentToString(FParameterMap InParamMap, EWebContentType InWebContentType)
{
	if(InParamMap.GetNum() == 0) return TEXT("");
	
	FString ContentStr;
	switch(InWebContentType)
	{
		case EWebContentType::Form:
		{
			for(auto& Iter : InParamMap.GetMap())
			{
				ContentStr.Append(FString::Printf(TEXT("%s=%s&"), *Iter.Key, *Iter.Value));
			}
			ContentStr.RemoveFromEnd(TEXT("&"));
			break;
		}
		case EWebContentType::Text:
		{
			for(auto& Iter : InParamMap.GetMap())
			{
				ContentStr.Append(FString::Printf(TEXT("%s=%s,"), *Iter.Key, *Iter.Value));
			}
			ContentStr.RemoveFromEnd(TEXT(","));
			break;
		}
		case EWebContentType::Json:
		{
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&ContentStr);
			JsonWriter->WriteObjectStart();
			for (auto& It : InParamMap.GetMap())
			{
				JsonWriter->WriteValue(It.Key, It.Value);
			}
			JsonWriter->WriteObjectEnd();
			JsonWriter->Close();
			break;
		}
	}
	return ContentStr;
}
