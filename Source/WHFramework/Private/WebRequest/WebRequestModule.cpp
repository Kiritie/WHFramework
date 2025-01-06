// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Common/CommonStatics.h"
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

void UWebRequestModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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
		return FWebRequestManager::Get().SendWebRequest(WebInterface->GetFullUrl(), InMethod, InHeadMap, InContent, [this, WebInterface, InContent, InParams](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			OnWebRequestComplete(Request, Response, bSuccess, WebInterface, InContent.ToString(), InParams);
		});
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
	InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded, HttpRequest, HttpResponse), InParams);
}
