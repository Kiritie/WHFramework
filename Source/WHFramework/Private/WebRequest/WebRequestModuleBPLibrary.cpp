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

bool UWebRequestModuleBPLibrary::HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->HasWebInterface(InWebInterfaceClass);
	}
	return false;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->GetWebInterface(InWebInterfaceClass);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->CreateWebInterface(InWebInterfaceClass);
	}
	return nullptr;
}

bool UWebRequestModuleBPLibrary::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->RegisterWebInterface(InWebInterfaceClass, InOnWebRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterWebInterface(InWebInterfaceClass, InOnWebRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterAllWebInterface(InWebInterfaceClass);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->ClearWebInterface(InWebInterfaceClass);
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

bool UWebRequestModuleBPLibrary::SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->SendWebRequest(InWebInterfaceClass, InMethod, InHeadMap, InContent);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AWebRequestModule::Get())
	{
		return WebRequestModule->K2_SendWebRequest(InWebInterfaceClass, InMethod, InHeadMap, InContent);
	}
	return false;
}
