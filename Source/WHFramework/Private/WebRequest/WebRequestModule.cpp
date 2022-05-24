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

	ServerURL = TEXT("");
	WebInterfaces = TArray<TSubclassOf<UWebInterfaceBase>>();
	WebInterfaceMap = TMap<TSubclassOf<UWebInterfaceBase>, UWebInterfaceBase*>();
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

	for(auto Iter : WebInterfaces)
	{
		CreateWebInterface(Iter);
	}
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

void AWebRequestModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	ClearAllWebInterface();
}

bool AWebRequestModule::HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return false;

	return WebInterfaceMap.Contains(InWebInterfaceClass);
}

UWebInterfaceBase* AWebRequestModule::GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(HasWebInterface(InWebInterfaceClass))
	{
		return WebInterfaceMap[InWebInterfaceClass];
	}
	return nullptr;
}

UWebInterfaceBase* AWebRequestModule::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(!HasWebInterface(InWebInterfaceClass))
	{
		UWebInterfaceBase* WebInterface = UObjectPoolModuleBPLibrary::SpawnObject<UWebInterfaceBase>(nullptr, InWebInterfaceClass);
		WebInterfaceMap.Add(InWebInterfaceClass, WebInterface);
		
		WHLog(WH_WebRequest, Log, TEXT("Succeeded to creating the interface: %s"), *WebInterface->GetName().ToString());
		
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
			
			WHLog(WH_WebRequest, Log, TEXT("Succeeded to register the interface: %s"), *WebInterface->GetName().ToString());
			
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

			WHLog(WH_WebRequest, Log, TEXT("Succeeded to un register the interface: %s"), *WebInterface->GetName().ToString());

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

		WHLog(WH_WebRequest, Log, TEXT("Succeeded to un register all the interface: %s"), *WebInterface->GetName().ToString());
		
		return true;
	}
	return false;
}

bool AWebRequestModule::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(!InWebInterfaceClass) return nullptr;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		WebInterfaceMap.Remove(InWebInterfaceClass);
		UObjectPoolModuleBPLibrary::DespawnObject(WebInterface);

		WHLog(WH_WebRequest, Log, TEXT("Succeeded to clear the interface: %s"), *WebInterface->GetName().ToString());
		
		return true;
	}
	return nullptr;
}

void AWebRequestModule::ClearAllWebInterface()
{
	for(auto Iter : WebInterfaceMap)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(Iter.Value);
	}
	WebInterfaceMap.Empty();
}

bool AWebRequestModule::SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(!InWebInterfaceClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InWebInterfaceClass))
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

		HttpRequest->SetURL(WebInterface->GetFullUrl());

		FString ContentType;
		switch(InContent.ContentType)
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
		InHeadMap.Add(TEXT("timestamp"), FString::FromInt(FDateTime::UtcNow().ToUnixTimestamp()));

		for(auto& Iter : InHeadMap.GetMap())
		{
			HttpRequest->SetHeader(Iter.Key, Iter.Value);
		}

		HttpRequest->SetContentAsString(InContent.ToString());

		HttpRequest->SetVerb(UGlobalBPLibrary::GetEnumValueDisplayName(TEXT("EWebRequestMethod"), (int32)InMethod).ToString());

		HttpRequest->OnProcessRequestComplete().BindUObject(this, &AWebRequestModule::OnWebRequestComplete, WebInterface, InContent.ToString());

		WHLog(WH_WebRequest, Log, TEXT("Start send web request: %s"), *WebInterface->GetName().ToString());
		WHLog(WH_WebRequest, Log, TEXT("------> Method: %s"), *HttpRequest->GetVerb());
		WHLog(WH_WebRequest, Log, TEXT("------> Head: %s"), *InHeadMap.ToString());
		WHLog(WH_WebRequest, Log, TEXT("------> Content: %s"), *InContent.ToString());

		return HttpRequest->ProcessRequest();
	}
	return false;
}

bool AWebRequestModule::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InWebInterfaceClass, InMethod, InHeadMap, InContent);
}

void AWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent)
{
	if(!HttpResponse.IsValid())
	{
		bSucceeded = false;
		WHLog(WH_WebRequest, Error, TEXT("Unable to process web request: %s"), *InWebInterface->GetName().ToString());
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
				WHLog(WH_WebRequest, Warning, TEXT("Web request: %s, Spendtime to long: %d"), *InWebInterface->GetName().ToString(), SpendTime);
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		WHLog(WH_WebRequest, Log, TEXT("Web request successed: %s"), *InWebInterface->GetName().ToString());
		WHLog(WH_WebRequest, Log, TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString());
	}
	else
	{
		bSucceeded = false;
		WHLog(WH_WebRequest, Error, TEXT("Web response returned error code: %d , Web request: %s"), HttpResponse->GetResponseCode(), *InWebInterface->GetName().ToString());
		WHLog(WH_WebRequest, Error, TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString());
	}

	InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded, HttpRequest, HttpResponse));
}
