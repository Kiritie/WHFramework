// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleStatics.h"

#include "WebRequest/WebRequestModule.h"

bool UWebRequestModuleStatics::IsWebLocalMode()
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->IsLocalMode();
	}
	return false;
}

void UWebRequestModuleStatics::SetWebLocalMode(bool bInLocalMode)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		WebRequestModule->SetLocalMode(bInLocalMode);
	}
}

FString UWebRequestModuleStatics::GetWebServerURL()
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->GetServerURL();
	}
	return TEXT("");
}

void UWebRequestModuleStatics::SetWebServerURL(const FString& InServerURL)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		WebRequestModule->SetServerURL(InServerURL);
	}
}

int32 UWebRequestModuleStatics::GetWebServerPort()
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->GetServerPort();
	}
	return 0;
}

void UWebRequestModuleStatics::SetWebServerPort(int32 InServerPort)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		WebRequestModule->SetServerPort(InServerPort);
	}
}

bool UWebRequestModuleStatics::HasWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->HasWebInterface(InClass);
	}
	return false;
}

UWebInterfaceBase* UWebRequestModuleStatics::GetWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->GetWebInterface(InClass);
	}
	return nullptr;
}

UWebInterfaceBase* UWebRequestModuleStatics::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->CreateWebInterface(InClass);
	}
	return nullptr;
}

bool UWebRequestModuleStatics::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->RegisterWebInterface(InClass, InOnRequestComplete);
	}
	return false;
}

bool UWebRequestModuleStatics::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterWebInterface(InClass, InOnRequestComplete);
	}
	return false;
}

bool UWebRequestModuleStatics::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->UnRegisterAllWebInterface(InClass);
	}
	return false;
}

bool UWebRequestModuleStatics::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->ClearWebInterface(InClass);
	}
	return false;
}

void UWebRequestModuleStatics::ClearAllWebInterface()
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		WebRequestModule->ClearAllWebInterface();
	}
}

bool UWebRequestModuleStatics::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
	}
	return false;
}

bool UWebRequestModuleStatics::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
	}
	return false;
}

bool UWebRequestModuleStatics::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	if(UWebRequestModule* WebRequestModule = UWebRequestModule::Get())
	{
		return WebRequestModule->K2_SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
	}
	return false;
}
