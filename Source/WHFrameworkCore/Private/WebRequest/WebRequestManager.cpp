// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/WebRequestManager.h"

#include "HttpModule.h"
#include "Debug/DebugTypes.h"
#include "Interfaces/IHttpResponse.h"
#include "Main/MainManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebRequest/FileDownloader/FileDownloaderInterface.h"

const FUniqueType FWebRequestManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FWebRequestManager)

// Sets default values
FWebRequestManager::FWebRequestManager() : FManagerBase(Type)
{
	bLocalMode = false;
	ServerIP = TEXT("");
	ServerPort = 0;
	bConnected = false;
	Downloaders = TArray<TSharedPtr<IFileDownloaderInterface>>();
}

FWebRequestManager::~FWebRequestManager()
{
	
}

void FWebRequestManager::OnInitialize()
{
	FManagerBase::OnInitialize();
}

void FWebRequestManager::OnPreparatory()
{
	FManagerBase::OnPreparatory();

	ConnectServer();
}

void FWebRequestManager::OnRefresh(float DeltaSeconds)
{
	FManagerBase::OnRefresh(DeltaSeconds);

	for(auto& Iter : TArray(Downloaders))
	{
		Iter->RefreshDownload(DeltaSeconds);
	}
}

void FWebRequestManager::ConnectServer()
{
	CancelWebRequest(GetServerURL());
	
	SendWebRequest(GetServerURL(), EWebRequestMethod::Get, {}, {}, [this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		bConnected = bSuccess;
	});
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL((InUrl.StartsWith(TEXT("/")) ? (GetServerURL() + InUrl) : InUrl) + (InContent.ContentType == EWebContentType::Form_Url ? (TEXT("?") + InContent.ToString()) : TEXT("")));

	WebRequestMap.Emplace(HttpRequest->GetURL(), HttpRequest.ToSharedPtr());

	FString ContentType;
	switch(InContent.ContentType)
	{
		case EWebContentType::Form_Url:
		case EWebContentType::Form_Body:
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

	for(auto& Iter : InHeadMap.GetSource())
	{
		HttpRequest->SetHeader(Iter.Key, Iter.Value);
	}

	if(InContent.ContentType != EWebContentType::Form_Url)
	{
		HttpRequest->SetContentAsString(InContent.ToString());
	}

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

	WHLog(TEXT("Start send web request"), EDC_WebRequest);
	WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest);
	WHLog(FString::Printf(TEXT("------> Method: %s"), *HttpRequest->GetVerb()), EDC_WebRequest);
	WHLog(FString::Printf(TEXT("------> Head: %s"), *InHeadMap.ToString()), EDC_WebRequest);
	WHLog(FString::Printf(TEXT("------> Content: %s"), *InContent.ToString()), EDC_WebRequest);

	return HttpRequest->ProcessRequest();
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithText OnComplete)
{
	return SendWebRequest(InUrl, InMethod, InHeadMap, InContent, [OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		OnComplete.Execute(bSuccess ? Response->GetContentAsString() : TEXT(""));
	});
}

bool FWebRequestManager::SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithJson OnComplete)
{
	return SendWebRequest(InUrl, InMethod, InHeadMap, InContent, [OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
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

bool FWebRequestManager::CancelWebRequest(const FString& InUrl)
{
	if(WebRequestMap.Contains(InUrl))
	{
		if(TSharedPtr<IHttpRequest> WebRequest = WebRequestMap[InUrl])
		{
			WebRequestMap.Remove(InUrl);
			WebRequest->CancelRequest();
		}
		return true;
	}
	return false;
}

TSharedPtr<IHttpRequest> FWebRequestManager::GetWebRequest(const FString& InUrl) const
{
	if(WebRequestMap.Contains(InUrl))
	{
		return WebRequestMap[InUrl];
	}
	return nullptr;
}

void FWebRequestManager::AddDownloader(const TSharedPtr<IFileDownloaderInterface>& Downloader)
{
	if(!Downloader) return;
	
	if(!Downloaders.Contains(Downloader))
	{
		Downloaders.Add(Downloader);
	}
}

void FWebRequestManager::RemoveDownloader(const TSharedPtr<IFileDownloaderInterface>& Downloader)
{
	if(!Downloader) return;;

	if(Downloaders.Contains(Downloader))
	{
		Downloaders.Remove(Downloader);
	}
}

void FWebRequestManager::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete)
{
	if(!WebRequestMap.Contains(HttpRequest->GetURL()))
	{
		WHLog(FString::Printf(TEXT("The web request has be canceled: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Warning);
		return;
	}

	WebRequestMap.Remove(HttpRequest->GetURL());

	if(!HttpResponse.IsValid())
	{
		WHLog(FString::Printf(TEXT("Unable to process web request: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Warning);
		OnComplete(HttpRequest, HttpResponse, false);
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
				WHLog(FString::Printf(TEXT("Web request spendtime to long: %d"), (int)SpendTime), EDC_WebRequest, EDV_Warning);
				WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Warning);
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		WHLog(TEXT("Web request successed"), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDC_WebRequest);
		OnComplete(HttpRequest, HttpResponse, true);
	}
	else
	{
		WHLog(FString::Printf(TEXT("Web response returned error code: %d"), HttpResponse->GetResponseCode()), EDC_WebRequest, EDV_Error);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Error);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDC_WebRequest, EDV_Error);
		OnComplete(HttpRequest, HttpResponse, false);
	}
}

FString FWebRequestManager::GetServerURL() const
{
	return FString::Printf(TEXT("%s:%d"), *ServerIP, ServerPort);
}

void FWebRequestManager::SetServerURL(const FString& InServerURL)
{
	const int32 MidIndex = InServerURL.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	ServerIP = InServerURL.Mid(0, MidIndex);
	ServerPort = FCString::Atoi(*InServerURL.Mid(MidIndex + 1));
}
