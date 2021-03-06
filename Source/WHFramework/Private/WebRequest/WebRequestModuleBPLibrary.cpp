// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "WebRequest/WebRequestModule.h"

bool UWebRequestModuleBPLibrary::IsWebLocalMode()
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->IsLocalMode();
	}
	return false;
}

void UWebRequestModuleBPLibrary::SetWebLocalMode(bool bInLocalMode)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		WebRequestModule->SetLocalMode(bInLocalMode);
	}
}

FString UWebRequestModuleBPLibrary::GetWebServerURL()
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->GetServerURL();
	}
	return TEXT("");
}

void UWebRequestModuleBPLibrary::SetWebServerURL(const FString& InServerURL)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		WebRequestModule->SetServerURL(InServerURL);
	}
}

int32 UWebRequestModuleBPLibrary::GetWebServerPort()
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->GetServerPort();
	}
	return 0;
}

void UWebRequestModuleBPLibrary::SetWebServerPort(int32 InServerPort)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		WebRequestModule->SetServerPort(InServerPort);
	}
}

bool UWebRequestModuleBPLibrary::HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->HasWebInterface(InWebInterfaceClass);
	}
	return false;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->GetWebInterface(InWebInterfaceClass);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModuleBPLibrary::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->CreateWebInterface(InWebInterfaceClass);
	}
	return nullptr;
}

bool UWebRequestModuleBPLibrary::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->RegisterWebInterface(InWebInterfaceClass, InOnWebRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->UnRegisterWebInterface(InWebInterfaceClass, InOnWebRequestComplete);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->UnRegisterAllWebInterface(InWebInterfaceClass);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->ClearWebInterface(InWebInterfaceClass);
	}
	return false;
}

void UWebRequestModuleBPLibrary::ClearAllWebInterface()
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		WebRequestModule->ClearAllWebInterface();
	}
}

bool UWebRequestModuleBPLibrary::SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->SendWebRequest(InWebInterfaceClass, InMethod, InHeadMap, InContent);
	}
	return false;
}

bool UWebRequestModuleBPLibrary::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent)
{
	if(AWebRequestModule* WebRequestModule = AMainModule::GetModuleByClass<AWebRequestModule>())
	{
		return WebRequestModule->K2_SendWebRequest(InWebInterfaceClass, InMethod, InHeadMap, InContent);
	}
	return false;
}
