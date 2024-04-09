// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/WebRequestManager.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Main/MainManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FUniqueType FWebRequestManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FWebRequestManager)

// Sets default values
FWebRequestManager::FWebRequestManager()
{
	bLocalMode = false;
	ServerURL = TEXT("http://192.168.2.177/airt_service");
	ServerPort = 8080;
}

FWebRequestManager::~FWebRequestManager()
{
	
}

FString FWebRequestManager::GetServerURL() const
{
	const int32 MidIndex = ServerURL.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 7);
	const FString BeginURL = !bLocalMode ? ServerURL.Mid(0, MidIndex) : TEXT("http://127.0.0.1");
	const FString EndURL = ServerURL.Mid(MidIndex, ServerURL.Len() - MidIndex);
	return FString::Printf(TEXT("%s:%d%s"), *BeginURL, ServerPort, *EndURL);
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithText OnComplete)
{
	return SendWebRequest(InUrl, InMethod, InHeadMap, InContent, [OnComplete](FHttpResponsePtr Response, bool bSuccess)
	{
		OnComplete.Execute(bSuccess ? Response->GetContentAsString() : TEXT(""));
	});
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithJson OnComplete)
{
	return SendWebRequest(InUrl, InMethod, InHeadMap, InContent, [OnComplete](FHttpResponsePtr Response, bool bSuccess)
	{
		TSharedPtr<FJsonObject> Json;
		if(bSuccess)
		{
			const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			FJsonSerializer::Deserialize(JsonReader, Json);
		}
		OnComplete.Execute(Json);
	});
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, TFunction<void(FHttpResponsePtr, bool)> OnComplete)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL(GetServerURL() + InUrl);

	FString ContentType;
	switch(InContent.ContentType)
	{
		case EWebContentType::Form:
		{
			ContentType = TEXT("application/x-www-form-urlencoded; charset=utf-8");
			break;
		}
		case EWebContentType::Text:
		{
			ContentType = TEXT("text/plain; charset=utf-8");
			break;
		}
		case EWebContentType::Json:
		{
			ContentType = TEXT("application/json; charset=utf-8");
			break;
		}
		case EWebContentType::Data:
		{
			ContentType = TEXT("multipart/form-data; charset=utf-8; boundary =---------------------------" + FString::FromInt(FDateTime::Now().GetTicks()));
			break;
		}
	}
	InHeadMap.Add(TEXT("Content-Type"), ContentType);
	InHeadMap.Add(TEXT("timestamp"), FString::FromInt(FDateTime::UtcNow().ToUnixTimestamp()));

	for(auto& Iter : InHeadMap.GetMap())
	{
		HttpRequest->SetHeader(Iter.Key, Iter.Value);
	}

	HttpRequest->SetContentAsString(InContent.ToString());

	switch(InMethod)
	{
		case EWebRequestMethod::Get:
		{
			HttpRequest->SetVerb("GET");
			break;
		}
		case EWebRequestMethod::Post:
		{
			HttpRequest->SetVerb("POST");
			break;
		}
	}

	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FWebRequestManager::OnWebRequestComplete, OnComplete);

	UE_LOG(LogTemp, Log, TEXT("Start send web request:"));
	UE_LOG(LogTemp, Log, TEXT("------> URL: %s"), *HttpRequest->GetURL());
	UE_LOG(LogTemp, Log, TEXT("------> Method: %s"), *HttpRequest->GetVerb());
	UE_LOG(LogTemp, Log, TEXT("------> Head: %s"), *InHeadMap.ToString());
	UE_LOG(LogTemp, Log, TEXT("------> Content: %s"), *InContent.ToString());

	return HttpRequest->ProcessRequest();
}

void FWebRequestManager::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<void(FHttpResponsePtr, bool)> OnComplete)
{
	if(!HttpResponse.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to process web request: %s"), *HttpRequest->GetURL());
		OnComplete(nullptr, false);
		return;
	}

	if(HttpRequest.IsValid())
	{
		FString Timestamp = HttpRequest->GetHeader("timestamp");
		if(!Timestamp.IsEmpty())
		{
			const int64 RequestTime = FCString::Atoi64(*Timestamp);
			const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
			const int64 SpendTime = Now - RequestTime;
			if(SpendTime > 8000)
			{
				UE_LOG(LogTemp, Warning, TEXT("Web request spendtime to long: %d"), (int)SpendTime);
				UE_LOG(LogTemp, Warning, TEXT("------> URL: %s"), *HttpRequest->GetURL());
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		UE_LOG(LogTemp, Log, TEXT("Web request successed"));
		UE_LOG(LogTemp, Log, TEXT("------> URL: %s"), *HttpRequest->GetURL());
		UE_LOG(LogTemp, Log, TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString());

		OnComplete(HttpResponse, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Web response returned error code: %d"), HttpResponse->GetResponseCode());
		UE_LOG(LogTemp, Warning, TEXT("------> URL: %s"), *HttpRequest->GetURL());
		UE_LOG(LogTemp, Warning, TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString());

		OnComplete(HttpResponse, false);
	}
}
