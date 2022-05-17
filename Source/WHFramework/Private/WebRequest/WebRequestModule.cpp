// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Parameter/ParameterModuleBPLibrary.h"
#include "WebRequest/Interface/Base/WebInterfaceBase.h"

// Sets default values
AWebRequestModule::AWebRequestModule()
{
	ModuleName = FName("WebRequestModule");

	WebInterfaces = TMap<TSubclassOf<UWebInterfaceBase>, UWebInterfaceBase*>();
}

#if WITH_EDITOR
void AWebRequestModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AWebRequestModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AWebRequestModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AWebRequestModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AWebRequestModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AWebRequestModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AWebRequestModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool AWebRequestModule::HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return false;

	return WebInterfaces.Contains(InWebInterfaceClass);
}

UWebInterfaceBase* AWebRequestModule::GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(HasWebInterface(InWebInterfaceClass))
	{
		WebInterfaces[InWebInterfaceClass];
	}
	return nullptr;
}

UWebInterfaceBase* AWebRequestModule::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(!HasWebInterface(InWebInterfaceClass))
	{
		UWebInterfaceBase* WebInterface = UObjectPoolModuleBPLibrary::SpawnObject<UWebInterfaceBase>(nullptr, InWebInterfaceClass);
		WebInterfaces.Add(InWebInterfaceClass, WebInterface);
		return WebInterface;
	}
	return nullptr;
}

bool AWebRequestModule::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = HasWebInterface(InWebInterfaceClass) ? GetWebInterface(InWebInterfaceClass) : CreateWebInterface(InWebInterfaceClass))
	{
		if(!WebInterface->OnWebRequestComplete.Contains(InOnWebRequestComplete))
		{
			WebInterface->OnWebRequestComplete.Add(InOnWebRequestComplete);
			return true;
		}
	}
	return false;
}

bool AWebRequestModule::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		if(WebInterface->OnWebRequestComplete.Contains(InOnWebRequestComplete))
		{
			WebInterface->OnWebRequestComplete.Remove(InOnWebRequestComplete);
			return true;
		}
	}
	return false;
}

bool AWebRequestModule::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		WebInterface->OnWebRequestComplete.Clear();
		return true;
	}
	return false;
}

bool AWebRequestModule::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		WebInterfaces.Remove(InWebInterfaceClass);
		UObjectPoolModuleBPLibrary::DespawnObject(WebInterface);
		return true;
	}
	return nullptr;
}

void AWebRequestModule::ClearAllWebInterface()
{
	for(auto Iter : WebInterfaces)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(Iter.Value);
	}
	WebInterfaces.Empty();
}

bool AWebRequestModule::SendWebRequestGet(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap)
{
	return SendWebRequestImpl(InWebInterfaceClass, InHeadMap);
}

bool AWebRequestModule::SendWebRequestPost(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap, const FParameterMap& InParamMap, EParameterMapType InParamMapType)
{
	FString ContentType;
	switch(InParamMapType)
	{
		case EParameterMapType::Text:
		{
			ContentType = TEXT("text/plain");
			break;
		}
		case EParameterMapType::Json:
		{
			ContentType = TEXT("application/json");
			break;
		}
	}
	InHeadMap.Add(TEXT("Content-Type"), ContentType);
	return SendWebRequestImpl(InWebInterfaceClass, InHeadMap, UParameterModuleBPLibrary::ParseParamMapToString(InParamMap, InParamMapType), true);
}

bool AWebRequestModule::SendWebRequestImpl(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap, const FString& InContent, bool bPost)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

		HttpRequest->SetURL(WebInterface->Url);

		InHeadMap.Add(TEXT("timestamp"), FString::FromInt(FDateTime::UtcNow().ToUnixTimestamp()));

		for(auto& Iter : InHeadMap)
		{
			HttpRequest->SetHeader(Iter.Key, Iter.Value);
		}

		HttpRequest->SetContentAsString(InContent);

		HttpRequest->SetVerb(!bPost ? TEXT("GET") : TEXT("POST"));

		HttpRequest->OnProcessRequestComplete().BindUObject(this, &AWebRequestModule::OnWebRequestComplete, WebInterface);

		return HttpRequest->ProcessRequest();
	}
	return false;
}

void AWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface)
{
	if(!HttpResponse.IsValid())
	{
		FString RequsetURL = HttpRequest.IsValid() ? HttpRequest->GetURL() : TEXT("");
		WHLog(WH_WebRequest, Error, TEXT("Unable to process web request : %s"), *RequsetURL);
		return;
	}

	if(HttpRequest.IsValid())
	{
		FString Timestamp = HttpRequest->GetHeader("timestamp");
		if(!Timestamp.IsEmpty())
		{
			const int64 RequestTime = FCString::Atoi64(*Timestamp);
			const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
			int64 SpendTime = Now - RequestTime;
			if(SpendTime > 8)
			{
				WHLog(WH_WebRequest, Warning, TEXT("URL: %s, Spendtime to long : %d"), *HttpResponse->GetURL(), SpendTime);
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		WHLog(WH_WebRequest, Log, TEXT("Web request successed ------> URL: %s"), *HttpResponse->GetURL());
		WHLog(WH_WebRequest, Log, TEXT("------> MessageBody %s"), *HttpResponse->GetURL(), *HttpResponse->GetContentAsString());
	}
	else
	{
		WHLog(WH_WebRequest, Error, TEXT("Web response returned error code: %d ------> URL: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetURL());
		WHLog(WH_WebRequest, Error, TEXT("------> MessageBody %s"), *HttpResponse->GetURL(), *HttpResponse->GetContentAsString());
		return;
	}

	if(InWebInterface)
	{
		InWebInterface->OnRequestComplete(FWebRequestInfo(bSucceeded, HttpRequest, HttpResponse));
	}
}
