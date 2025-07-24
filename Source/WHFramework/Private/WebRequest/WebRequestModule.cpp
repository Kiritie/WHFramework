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
	ServerIP = TEXT("");
	ServerPort = 8080;
	
	WebInterfaces = TArray<UWebInterfaceBase*>();
	WebRequestConfigs = TArray<FWebRequestConfig>();
	WebInterfaceMap = TMap<FName, UWebInterfaceBase*>();
	WebRequestConfigMap = TMap<FName, FWebRequestConfig>();
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

	FWebRequestManager::Get().SetLocalMode(bLocalMode);
	FWebRequestManager::Get().SetServerIP(ServerIP);
	FWebRequestManager::Get().SetServerPort(ServerPort);

	for(auto& Iter : WebInterfaces)
	{
		CreateWebInterface(Iter);
	}
	for(auto& Iter : WebRequestConfigs)
	{
		CreateWebRequestConfig(Iter.Name, Iter.Method, Iter.Params, Iter.HeadMap, Iter.Content, Iter.TriggerType, Iter.TriggerTime);
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

void UWebRequestModule::SetLocalMode(bool bInLocalMode)
{
	bLocalMode = bInLocalMode;
	FWebRequestManager::Get().SetLocalMode(bLocalMode);
}

void UWebRequestModule::SetServerIP(const FString& InServerIP)
{
	ServerIP = InServerIP;
	FWebRequestManager::Get().SetServerIP(ServerIP);
}

void UWebRequestModule::SetServerPort(int32 InServerPort)
{
	ServerPort = InServerPort;
	FWebRequestManager::Get().SetServerPort(ServerPort);
}

FString UWebRequestModule::GetServerURL() const
{
	return FWebRequestManager::Get().GetServerURL();
}

void UWebRequestModule::SetServerURL(const FString& InServerURL)
{
	FWebRequestManager::Get().SetServerURL(InServerURL);
	ServerIP = FWebRequestManager::Get().GetServerIP();
	ServerPort = FWebRequestManager::Get().GetServerPort();
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

bool UWebRequestModule::HasWebRequestConfig(const FName InName)
{
	return WebRequestConfigMap.Contains(InName);
}

FWebRequestConfig UWebRequestModule::GetWebRequestConfig(const FName InName)
{
	if(HasWebRequestConfig(InName))
	{
		return WebRequestConfigMap[InName];
	}
	return FWebRequestConfig();
}

FWebRequestConfig UWebRequestModule::CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return CreateWebRequestConfig(InName, InMethod, InParams ? *InParams : TArray<FParameter>(), InHeadMap, InContent, InTriggerType, InTriggerTime);
}

FWebRequestConfig UWebRequestModule::CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	FWebRequestConfig Config;
	if(!HasWebRequestConfig(InName))
	{
		Config.Name = InName;
		Config.Method = InMethod;
		Config.Params = InParams;
		Config.HeadMap = InHeadMap;
		Config.Content = InContent;
		Config.TriggerType = InTriggerType;
		Config.TriggerTime = InTriggerTime;

		switch(Config.TriggerType)
		{
			case EWebRequestTriggerType::Timer:
			{
				GetWorld()->GetTimerManager().SetTimer(Config.TriggerTimerHandle, FTimerDelegate::CreateLambda([this, Config]()
				{
					SendWebRequest(Config.Name, Config.Method, Config.Params, Config.HeadMap, Config.Content);
				}), Config.TriggerTime, true);
			}
			case EWebRequestTriggerType::Default:
			{
				GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([this, Config]()
				{
					SendWebRequest(Config.Name, Config.Method, Config.Params, Config.HeadMap, Config.Content);
				}));
				break;
			}
			default: break;
		}

		WebRequestConfigMap.Add(Config.Name, Config);
	}
	return Config;
}

FWebRequestConfig UWebRequestModule::K2_CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime);
}

bool UWebRequestModule::ClearWebRequestConfig(const FName InName)
{
	if(HasWebRequestConfig(InName))
	{
		FWebRequestConfig& Config = WebRequestConfigMap[InName];
		if(Config.TriggerType == EWebRequestTriggerType::Timer)
		{
			GetWorld()->GetTimerManager().ClearTimer(Config.TriggerTimerHandle);
		}
		WebRequestConfigMap.Remove(InName);
		return true;
	}
	return false;
}

void UWebRequestModule::ClearAllWebRequestConfig()
{
	for(auto& Iter : WebRequestConfigMap)
	{
		ClearWebRequestConfig(Iter.Key);
	}
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
			if(UCommonStatics::IsPlaying())
			{
				OnWebRequestComplete(Request, Response, bSuccess, WebInterface, InContent.ToString(), InParams);
			}
		});
	}
	else
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Send seb request failed, interface dose not exist: %s"), *InName.ToString()), EDC_WebRequest, EDV_Error);
	}
	return false;
}

bool UWebRequestModule::K2_SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModule::SendWebRequestByConfig(const FName InName)
{
	if(WebRequestConfigMap.Contains(InName))
	{
		const FWebRequestConfig& Config = WebRequestConfigMap[InName];
		if(Config.TriggerType == EWebRequestTriggerType::Procedure)
		{
			return SendWebRequest(InName, Config.Method, Config.Params, Config.HeadMap, Config.Content);
		}
	}
	return false;
}

void UWebRequestModule::OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent, const TArray<FParameter> InParams)
{
	InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded, HttpRequest, HttpResponse), InParams);
}
