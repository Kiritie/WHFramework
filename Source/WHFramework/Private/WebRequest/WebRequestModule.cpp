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

	ServerURL = TEXT("http://127.0.0.1:8080/survival_world_war_exploded");
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
		return WebInterfaces[InWebInterfaceClass];
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
		if(!WebInterface->GetOnWebRequestComplete().Contains(InOnWebRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Add(InOnWebRequestComplete);
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
		if(WebInterface->GetOnWebRequestComplete().Contains(InOnWebRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Remove(InOnWebRequestComplete);
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
		WebInterface->GetOnWebRequestComplete().Clear();
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

bool AWebRequestModule::SendWebRequestGet(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap InHeadMap)
{
	return SendWebRequestImpl(InWebInterfaceClass, InHeadMap);
}

bool AWebRequestModule::SendWebRequestPost(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap InHeadMap, FWebContent InWebContent)
{
	FString ContentType;
	switch(InWebContent.ContentType)
	{
		case EWebContentType::Form:
		{
			ContentType = TEXT("application/x-www-form-urlencoded");
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
	}
	InHeadMap.Add(TEXT("Content-Type"), ContentType);
	return SendWebRequestImpl(InWebInterfaceClass, InHeadMap, InWebContent.ToString(), true);
}

bool AWebRequestModule::SendWebRequestImpl(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap InHeadMap, const FString& InContent, bool bPost)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

		HttpRequest->SetURL(ServerURL + WebInterface->GetUrl());

		InHeadMap.Add(TEXT("timestamp"), FString::FromInt(FDateTime::UtcNow().ToUnixTimestamp()));

		for(auto& Iter : InHeadMap.GetMap())
		{
			HttpRequest->SetHeader(Iter.Key, Iter.Value);
		}

		HttpRequest->SetContentAsString(InContent);

		HttpRequest->SetVerb(!bPost ? TEXT("GET") : TEXT("POST"));

		HttpRequest->OnProcessRequestComplete().BindUObject(this, &AWebRequestModule::OnWebRequestComplete, WebInterface, InContent);

		return HttpRequest->ProcessRequest();
	}
	return false;
}

void AWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent)
{
	if(!HttpResponse.IsValid())
	{
		bSucceeded = false;
		FString RequsetURL = HttpRequest.IsValid() ? HttpRequest->GetURL() : TEXT("");
		WHLog(WH_WebRequest, Error, TEXT("Unable to process web request : %s"), *InWebInterface->GetName().ToString());
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
		bSucceeded = false;
		WHLog(WH_WebRequest, Error, TEXT("Web response returned error code: %d ------> URL: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetURL());
		WHLog(WH_WebRequest, Error, TEXT("------> MessageBody %s"), *HttpResponse->GetURL(), *HttpResponse->GetContentAsString());
	}

	if(InWebInterface)
	{
		InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded, HttpRequest, HttpResponse));
	}
}
