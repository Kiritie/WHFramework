// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"
#include "Common/CommonTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"

#include "Debug/DebugModuleTypes.h"
#include "Common/CommonModuleStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "WebRequest/Interface/Base/WebInterfaceBase.h"
#include "Engine/World.h"
		
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
}

#if WITH_EDITOR
void UWebRequestModule::OnGenerate()
{
	Super::OnGenerate();
}

void UWebRequestModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void UWebRequestModule::OnInitialize()
{
	Super::OnInitialize();

	bStopping = false;
	LifecycleEpoch = FGuid::NewGuid();

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
		bStopping = true;
		LifecycleEpoch.Invalidate();
		ClearAllWebRequestConfig();
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
}

void UWebRequestModule::SetServerIP(const FString& InServerIP)
{
	ServerIP = InServerIP;
}

void UWebRequestModule::SetServerPort(int32 InServerPort)
{
	ServerPort = InServerPort;
}

FString UWebRequestModule::GetServerURL() const
{
	return ServerIP.IsEmpty() || ServerPort <= 0 ? FString() : FString::Printf(TEXT("%s:%d"), *ServerIP, ServerPort);
}

void UWebRequestModule::SetServerURL(const FString& InServerURL)
{
	int32 Separator = INDEX_NONE;
	if (!InServerURL.FindLastChar(TEXT(':'), Separator) || Separator <= 0 || Separator == InServerURL.Len() - 1) return;
	const FString PortText = InServerURL.Mid(Separator + 1);
	if (!PortText.IsNumeric()) return;
	const int32 Port = FCString::Atoi(*PortText);
	if (Port <= 0 || Port > 65535) return;
	ServerIP = InServerURL.Left(Separator);
	ServerPort = Port;
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
	if (bStopping || bClearingInterfaces || !LifecycleEpoch.IsValid() || !InClass) return nullptr;
	if(!HasWebInterface(InName))
	{
		UWebInterfaceBase* Interface = UObjectPoolModuleStatics::SpawnObject<UWebInterfaceBase>(this, InClass);
		if (Interface && Interface->GetNameN() == InName)
		{
			if (UWebInterfaceBase* Created = CreateWebInterface(Interface)) return Created;
		}
		if (Interface) UObjectPoolModuleStatics::DespawnObject(this, Interface);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModule::CreateWebInterface(UWebInterfaceBase* InWebInterface)
{
	if (bStopping || bClearingInterfaces || !LifecycleEpoch.IsValid() || !::IsValid(InWebInterface) || InWebInterface->GetWorld() != GetWorld()) return nullptr;
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
		TArray<FGuid> ToCancel;
		for (const auto& Entry : OwnedRequests) if (Entry.Value == InName) ToCancel.Add(Entry.Key);
		for (const FGuid& RequestId : ToCancel)
		{
			OwnedRequests.Remove(RequestId);
			if (FWebRequestManager* Manager = FWebRequestManager::GetPtr()) Manager->CancelWebRequest(RequestId);
		}
		WebInterfaceMap.Remove(InName);
		UObjectPoolModuleStatics::DespawnObject(this, WebInterface);

		WHLog(FString::Printf(TEXT("Succeeded to clear the interface: %s"), *InName.ToString()), EDC_WebRequest);
		
		return true;
	}
	return false;
}

void UWebRequestModule::ClearAllWebInterface()
{
	if (bClearingInterfaces) return;
	const TGuardValue<bool> Guard(bClearingInterfaces, true);
	TArray<FName> Names;
	WebInterfaceMap.GetKeys(Names);
	for (const FName Name : Names) ClearWebInterface(Name);
	TArray<FGuid> RemainingRequests;
	OwnedRequests.GetKeys(RemainingRequests);
	OwnedRequests.Reset();
	if (FWebRequestManager* Manager = FWebRequestManager::GetPtr())
		for (const FGuid& RequestId : RemainingRequests) Manager->CancelWebRequest(RequestId);
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
	UWorld* World = GetWorld();
	if (bStopping || !LifecycleEpoch.IsValid() || !World || World->bIsTearingDown || InName.IsNone() || HasWebRequestConfig(InName) ||
		(InTriggerType == EWebRequestTriggerType::Timer && (!FMath::IsFinite(InTriggerTime) || InTriggerTime <= 0.f))) return FWebRequestConfig();

	FWebRequestConfig Config;
	Config.Name = InName;
	Config.Method = InMethod;
	Config.Params = InParams;
	Config.HeadMap = InHeadMap;
	Config.Content = InContent;
	Config.TriggerType = InTriggerType;
	Config.TriggerTime = InTriggerTime;
	const FGuid Generation = FGuid::NewGuid();
	ConfigGenerations.Add(InName, Generation);
	WebRequestConfigMap.Add(InName, Config);
	const TWeakObjectPtr<UWebRequestModule> WeakOwner(this);
	const FGuid OriginalLifecycle = LifecycleEpoch;
	const FTimerDelegate Trigger = FTimerDelegate::CreateLambda([WeakOwner, OriginalLifecycle, Generation, InName]()
	{
		UWebRequestModule* Owner = WeakOwner.Get();
		if (!Owner || Owner->bStopping || Owner->LifecycleEpoch != OriginalLifecycle ||
			Owner->ConfigGenerations.FindRef(InName) != Generation) return;
		const FWebRequestConfig* Found = Owner->WebRequestConfigMap.Find(InName);
		if (!Found) return;
		const FWebRequestConfig Snapshot = *Found;
		Owner->SendWebRequest(Snapshot.Name, Snapshot.Method, Snapshot.Params, Snapshot.HeadMap, Snapshot.Content);
	});
	if (InTriggerType == EWebRequestTriggerType::Timer)
		World->GetTimerManager().SetTimer(Config.TriggerTimerHandle, Trigger, InTriggerTime, true);
	else if (InTriggerType == EWebRequestTriggerType::Default)
		Config.TriggerTimerHandle = World->GetTimerManager().SetTimerForNextTick(Trigger);
	if (FWebRequestConfig* Stored = WebRequestConfigMap.Find(InName)) Stored->TriggerTimerHandle = Config.TriggerTimerHandle;
	return Config;
}

FWebRequestConfig UWebRequestModule::K2_CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime);
}

bool UWebRequestModule::ClearWebRequestConfig(const FName InName)
{
	FWebRequestConfig Config;
	if (!WebRequestConfigMap.RemoveAndCopyValue(InName, Config)) return false;
	ConfigGenerations.Remove(InName);
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(Config.TriggerTimerHandle);
	return true;
}

void UWebRequestModule::ClearAllWebRequestConfig()
{
	TArray<FName> Names;
	WebRequestConfigMap.GetKeys(Names);
	for (const FName Name : Names) ClearWebRequestConfig(Name);
	ConfigGenerations.Empty();
}

bool UWebRequestModule::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return SendWebRequest(InName, InMethod, InParams ? *InParams : TArray<FParameter>(), InHeadMap, InContent);
}

bool UWebRequestModule::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	if (bStopping || bClearingInterfaces || !LifecycleEpoch.IsValid() || !GetWorld() || GetWorld()->bIsTearingDown) return false;
	if(UWebInterfaceBase* WebInterface = GetWebInterface(InName))
	{
        const TWeakObjectPtr<UWebRequestModule> WeakOwner(this);
        const TWeakObjectPtr<UWebInterfaceBase> WeakInterface(WebInterface);
        const FGuid OriginalLifecycle = LifecycleEpoch;
        const FString FullUrl = WebInterface->GetFullUrl();
        if (!FullUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) && !FullUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)) return false;
        const FGuid RequestId = FGuid::NewGuid();
        OwnedRequests.Add(RequestId, InName);
        const FGuid SubmittedId = FWebRequestManager::Get().SendWebRequest(FullUrl, InMethod, InHeadMap, InContent,
            [WeakOwner, WeakInterface, OriginalLifecycle, RequestId, InName, InContent, InParams](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
            {
                UWebRequestModule* Owner = WeakOwner.Get();
                UWebInterfaceBase* Interface = WeakInterface.Get();
                if (!Owner || !Owner->OwnedRequests.Remove(RequestId) || !Interface || Owner->bStopping || Owner->LifecycleEpoch != OriginalLifecycle ||
                    Owner->GetModuleState() == EModuleState::Terminated || Owner->GetWebInterface(InName) != Interface) return;
                Owner->OnWebRequestComplete(Request, Response, bSuccess, Interface, InContent.ToString(), InParams);
            }, RequestId);
        if (SubmittedId != RequestId)
        {
            OwnedRequests.Remove(RequestId);
            return false;
        }
        return true;
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
	InWebInterface->RequestComplete(FWebRequestResult(InContent, bSucceeded && HttpResponse.IsValid(), HttpRequest, HttpResponse), InParams);
}
