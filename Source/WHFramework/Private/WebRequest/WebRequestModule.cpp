// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"

#include "HttpModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Common/CommonStatics.h"
#include "Interfaces/IHttpResponse.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "WebRequest/Interface/Base/WebInterfaceBase.h"
		
IMPLEMENTATION_MODULE(UWebRequestModule)

// Sets default values
UWebRequestModule::UWebRequestModule()
{
	ModuleName = FName("WebRequestModule");
	ModuleDisplayName = FText::FromString(TEXT("Web Request Module"));

	bLocalMode = false;
	ServerURL = TEXT("");
	ServerPort = 8080;
	
	WebInterfaces = TArray<UWebInterfaceBase*>();
	WebInterfaceMap = TMap<FName, UWebInterfaceBase*>();
}

UWebRequestModule::~UWebRequestModule()
{
	TERMINATION_MODULE(UWebRequestModule)
}

#if WITH_EDITOR
void UWebRequestModule::OnGenerate()
{
	Super::OnGenerate();
}

void UWebRequestModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UWebRequestModule)
}
#endif

void UWebRequestModule::OnInitialize()
{
	Super::OnInitialize();

	for(auto& Iter : WebInterfaces)
	{
		CreateWebInterface(Iter);
	}
}

void UWebRequestModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UWebRequestModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UWebRequestModule::OnPause()
{
	Super::OnPause();
}

void UWebRequestModule::OnUnPause()
{
	Super::OnUnPause();
}

void UWebRequestModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		ClearAllWebInterface();
	}
}

FString UWebRequestModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

FString UWebRequestModule::GetServerURL() const
{
	const int32 MidIndex = ServerURL.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 7);
	const FString BeginURL = !bLocalMode ? ServerURL.Mid(0, MidIndex) : TEXT("http://127.0.0.1");
	const FString EndURL = ServerURL.Mid(MidIndex, ServerURL.Len() - MidIndex);
	return FString::Printf(TEXT("%s:%d%s"), *BeginURL, ServerPort, *EndURL);
}

bool UWebRequestModule::HasWebInterface(const FName InName)
{
	return WebInterfaceMap.Contains(InName);
}

UWebInterfaceBase* UWebRequestModule::GetWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	if(HasWebInterface(InName))
	{
		return GetDeterminesOutputObject(WebInterfaceMap[InName], InClass);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModule::CreateWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	if(!HasWebInterface(InName))
	{
		return CreateWebInterface(UObjectPoolModuleStatics::SpawnObject<UWebInterfaceBase>(nullptr, nullptr, InClass));
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModule::CreateWebInterface(UWebInterfaceBase* InWebInterface)
{
	if(!HasWebInterface(InWebInterface->GetNameN()))
	{
		WebInterfaceMap.Add(InWebInterface->GetNameN(), InWebInterface);
		
		WHLog(FString::Printf(TEXT("Succeeded to creating the interface: %s"), *InWebInterface->GetNameN().ToString()), EDC_WebRequest);
		
		return InWebInterface;
	}
	return nullptr;
}

bool UWebRequestModule::RegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
	{
		if(!WebInterface->GetOnWebRequestComplete().Contains(InOnRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Add(InOnRequestComplete);
			
			WHLog(FString::Printf(TEXT("Succeeded to register the interface: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest);
			
			return true;
		}
	}
	return false;
}

bool UWebRequestModule::UnRegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
	{
		if(WebInterface->GetOnWebRequestComplete().Contains(InOnRequestComplete))
		{
			WebInterface->GetOnWebRequestComplete().Remove(InOnRequestComplete);

			WHLog(FString::Printf(TEXT("Succeeded to un register the interface: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest);

			return true;
		}
	}
	return false;
}

bool UWebRequestModule::UnRegisterAllWebInterface(const FName InName)
{
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
	{
		WebInterface->GetOnWebRequestComplete().Clear();

		WHLog(FString::Printf(TEXT("Succeeded to un register all the interface: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest);
		
		return true;
	}
	return false;
}

bool UWebRequestModule::ClearWebInterface(const FName InName)
{
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
	{
		WebInterfaceMap.Remove(InName);
		UObjectPoolModuleStatics::DespawnObject(WebInterface);

		WHLog(FString::Printf(TEXT("Succeeded to clear the interface: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest);
		
		return true;
	}
	return false;
}

void UWebRequestModule::ClearAllWebInterface()
{
	for(auto Iter : WebInterfaceMap)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Value);
	}
	WebInterfaceMap.Empty();
}

bool UWebRequestModule::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InName, InMethod, InParams ? *InParams : TArray<FParameter>(), InHeadMap, InContent);
}

bool UWebRequestModule::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
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

		HttpRequest->SetVerb(UCommonStatics::GetEnumValueDisplayName(TEXT("/Script/WHFramework.EWebRequestMethod"), (int32)InMethod).ToString());

		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UWebRequestModule::OnWebRequestComplete, WebInterface, InContent.ToString(), InParams);

		WHLog(FString::Printf(TEXT("Start send web request: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> Method: %s"), *HttpRequest->GetVerb()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> Head: %s"), *InHeadMap.ToString()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> Content: %s"), *InContent.ToString()), EDC_WebRequest);

		return HttpRequest->ProcessRequest();
	}
	else
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Send seb request failed, interface dose not exist: %s"), *WebInterface->GetNameN().ToString()), EDC_WebRequest, EDV_Error);
	}
	return false;
}

bool UWebRequestModule::K2_SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent);
}

void UWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent, const TArray<FParameter> InParams)
{
	if(!HttpResponse.IsValid())
	{
		WHLog(FString::Printf(TEXT("Unable to process web request: %s"), *InWebInterface->GetNameN().ToString()), EDC_WebRequest, EDV_Warning);
		
		InWebInterface->RequestComplete(FWebRequestResult(InContent, false, HttpRequest, HttpResponse), InParams);
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
				WHLog(FString::Printf(TEXT("Web request: %s, Spendtime to long: %d"), *InWebInterface->GetNameN().ToString(), (int)SpendTime), EDC_WebRequest, EDV_Warning);
				WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Warning);
			}
		}
	}

	if(EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		WHLog(FString::Printf(TEXT("Web request successed: %s"), *InWebInterface->GetNameN().ToString()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDC_WebRequest);
		
		InWebInterface->RequestComplete(FWebRequestResult(InContent, true, HttpRequest, HttpResponse), InParams);
	}
	else
	{
		WHLog(FString::Printf(TEXT("Web response returned error code: %d , Web request: %s"), HttpResponse->GetResponseCode(), *InWebInterface->GetNameN().ToString()), EDC_WebRequest, EDV_Error);
		WHLog(FString::Printf(TEXT("------> URL: %s"), *HttpRequest->GetURL()), EDC_WebRequest, EDV_Error);
		WHLog(FString::Printf(TEXT("------> Message body: %s"), *HttpResponse->GetContentAsString()), EDC_WebRequest, EDV_Error);
		
		InWebInterface->RequestComplete(FWebRequestResult(InContent, false, HttpRequest, HttpResponse), InParams);
	}
}
