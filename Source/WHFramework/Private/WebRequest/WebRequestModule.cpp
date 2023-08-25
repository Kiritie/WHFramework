// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"

#include "HttpModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Global/GlobalBPLibrary.h"
#include "Interfaces/IHttpResponse.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Parameter/ParameterModuleBPLibrary.h"
#include "WebRequest/Interface/Base/WebInterfaceBase.h"
		
IMPLEMENTATION_MODULE(AWebRequestModule)

// Sets default values
AWebRequestModule::AWebRequestModule()
{
	ModuleName = FName("WebRequestModule");

	bLocalMode = false;
	ServerURL = TEXT("");
	ServerPort = 8080;
	
	WebInterfaces = TArray<TSubclassOf<UWebInterfaceBase>>();
	WebInterfaceMap = TMap<TSubclassOf<UWebInterfaceBase>, UWebInterfaceBase*>();
}

AWebRequestModule::~AWebRequestModule()
{
	TERMINATION_MODULE(AWebRequestModule)
}

#if WITH_EDITOR
void AWebRequestModule::OnGenerate()
{
	Super::OnGenerate();
}

void AWebRequestModule::OnDestroy()
{
	Super::OnDestroy();
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

void AWebRequestModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
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

void AWebRequestModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(InPhase == EPhase::Primary)
	{
		ClearAllWebInterface();
	}
}

FString AWebRequestModule::GetServerURL() const
{
	const int32 MidIndex = ServerURL.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 7);
	const FString BeginURL = !bLocalMode ? ServerURL.Mid(0, MidIndex) : TEXT("http://127.0.0.1");
	const FString EndURL = ServerURL.Mid(MidIndex, ServerURL.Len() - MidIndex);
	return FString::Printf(TEXT("%s:%d%s"), *BeginURL, ServerPort, *EndURL);
}

bool AWebRequestModule::HasWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!InClass) return false;

	return WebInterfaceMap.Contains(InClass);
}

UWebInterfaceBase* AWebRequestModule::GetWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!InClass) return nullptr;

	if(HasWebInterface(InClass))
	{
		return WebInterfaceMap[InClass];
	}
	return nullptr;
}

UWebInterfaceBase* AWebRequestModule::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!InClass) return nullptr;

	if(!HasWebInterface(InClass))
	{
		UWebInterfaceBase* WebInterface = UObjectPoolModuleBPLibrary::SpawnObject<UWebInterfaceBase>(nullptr, InClass);
		WebInterfaceMap.Add(InClass, WebInterface);
		
		WHLog(FString::Printf(TEXT("Succeeded to creating the interface: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);
		
		return WebInterface;
	}
	return nullptr;
}

bool AWebRequestModule::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(!InClass) return false;

	if(UWebInterfaceBase* WebInterface = HasWebInterface(InClass) ? GetWebInterface(InClass) : CreateWebInterface(InClass))
	{
		if(!WebInterface->GetOnWebRequestComplete().Contains(InOnRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Add(InOnRequestComplete);
			
			WHLog(FString::Printf(TEXT("Succeeded to register the interface: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);
			
			return true;
		}
	}
	return false;
}

bool AWebRequestModule::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(!InClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InClass))
	{
		if(WebInterface->GetOnWebRequestComplete().Contains(InOnRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Remove(InOnRequestComplete);

			WHLog(FString::Printf(TEXT("Succeeded to un register the interface: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);

			return true;
		}
	}
	return false;
}

bool AWebRequestModule::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!InClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InClass))
	{
		WebInterface->GetOnWebRequestComplete().Clear();

		WHLog(FString::Printf(TEXT("Succeeded to un register all the interface: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);
		
		return true;
	}
	return false;
}

bool AWebRequestModule::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!InClass) return nullptr;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InClass))
	{
		WebInterfaceMap.Remove(InClass);
		UObjectPoolModuleBPLibrary::DespawnObject(WebInterface);

		WHLog(FString::Printf(TEXT("Succeeded to clear the interface: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);
		
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

bool AWebRequestModule::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(!InClass) return false;

	if(UWebInterfaceBase* WebInterface = GetWebInterface(InClass))
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

		HttpRequest->SetURL(WebInterface->GetFullUrl());

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
		}
		InHeadMap.Add(TEXT("Content-Type"), ContentType);
		InHeadMap.Add(TEXT("timestamp"), FString::FromInt(FDateTime::UtcNow().ToUnixTimestamp()));

		for(auto& Iter : InHeadMap.GetMap())
		{
			HttpRequest->SetHeader(Iter.Key, Iter.Value);
		}

		HttpRequest->SetContentAsString(InContent.ToString());

		HttpRequest->SetVerb(UGlobalBPLibrary::GetEnumValueDisplayName(TEXT("/Script/WHFramework.EWebRequestMethod"), (int32)InMethod).ToString());

		HttpRequest->OnProcessRequestComplete().BindUObject(this, &AWebRequestModule::OnWebRequestComplete, WebInterface, InContent.ToString());

		WHLog(FString::Printf(TEXT("Start send web request: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> Method: %s"), *HttpRequest->GetVerb()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> Head: %s"), *InHeadMap.ToString()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> Content: %s"), *InContent.ToString()), EDebugCategory::WebRequest);

		return HttpRequest->ProcessRequest();
	}
	else
	{
		ensureEditor(true);
		WHLog(FString::Printf(TEXT("Send seb request failed, interface dose not exist: %s"), *WebInterface->GetName().ToString()), EDebugCategory::WebRequest, EDebugVerbosity::Warning);
	}
	return false;
}

bool AWebRequestModule::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InClass, InMethod, InHeadMap, InContent);
}

void AWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent)
{
	if(!HttpResponse.IsValid())
	{
		bSucceeded = false;
		WHLog(FString::Printf(TEXT("Unable to process web request: %s"), *InWebInterface->GetName().ToString()), EDebugCategory::WebRequest, EDebugVerbosity::Error);
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
				WHLog(FString::Printf(TEXT("Web request: %s, Spendtime to long: %d"), *InWebInterface->GetName().ToString(), (int)SpendTime), EDebugCategory::WebRequest, EDebugVerbosity::Warning);
				WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDebugCategory::WebRequest, EDebugVerbosity::Warning);
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		WHLog(FString::Printf(TEXT("Web request successed: %s"), *InWebInterface->GetName().ToString()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDebugCategory::WebRequest);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDebugCategory::WebRequest);
	}
	else
	{
		bSucceeded = false;
		WHLog(FString::Printf(TEXT("Web response returned error code: %d , Web request: %s"), HttpResponse->GetResponseCode(), *InWebInterface->GetName().ToString()), EDebugCategory::WebRequest, EDebugVerbosity::Error);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDebugCategory::WebRequest, EDebugVerbosity::Error);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDebugCategory::WebRequest, EDebugVerbosity::Error);
	}

	InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded, HttpRequest, HttpResponse));
}
