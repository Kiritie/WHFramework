// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "WebRequest/WebRequestModule.h"

bool UWebRequestModuleBPLibrary::IsWebLocalMode()
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->IsLocalMode();
	}
	return false;
}

void UWebRequestModuleBPLibrary::SetWebLocalMode(bool bInLocalMode)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		WebRequestModule->SetLocalMode(bInLocalMode);
	}
}

FString UWebRequestModuleBPLibrary::GetWebServerURL()
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->GetServerURL();
	}
	return TEXT("");
}

void UWebRequestModuleBPLibrary::SetWebServerURL(const FString& InServerURL)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		WebRequestModule->SetServerURL(InServerURL);
	}
}

int32 UWebRequestModuleBPLibrary::GetWebServerPort()
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->GetServerPort();
	}
	return 0;
}

void UWebRequestModuleBPLibrary::SetWebServerPort(int32 InServerPort)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		WebRequestModule->SetServerPort(InServerPort);
	}
}

bool UWebRequestModuleBPLibrary::HasWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->HasWebInterface(InClass);
	}
	return false;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::GetWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->GetWebInterface(InClass);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->CreateWebInterface(InClass);
	}
	return nullptr;
}

bool UWebRequestModuleBPLibrary::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->RegisterWebInterface(InClass, InOnRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterWebInterface(InClass, InOnRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterAllWebInterface(InClass);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->ClearWebInterface(InClass);
	}
	return false;
}

void UWebRequestModuleBPLibrary::ClearAllWebInterface()
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		WebRequestModule->ClearAllWebInterface();
	}
}

bool UWebRequestModuleBPLibrary::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->SendWebRequest(InClass, InMethod, InHeadMap, InContent);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->K2_SendWebRequest(InClass, InMethod, InHeadMap, InContent);
	}
	return false;
}
