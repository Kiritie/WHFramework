// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleStatics.h"

#include "WebRequest/WebRequestModule.h"

bool UWebRequestModuleStatics::IsWebLocalMode()
{
	return UWebRequestModule::Get().IsLocalMode();
}

void UWebRequestModuleStatics::SetWebLocalMode(bool bInLocalMode)
{
	UWebRequestModule::Get().SetLocalMode(bInLocalMode);
}

FString UWebRequestModuleStatics::GetWebServerURL()
{
	return UWebRequestModule::Get().GetServerURL();
}

void UWebRequestModuleStatics::SetWebServerURL(const FString& InServerURL)
{
	UWebRequestModule::Get().SetServerURL(InServerURL);
}

int32 UWebRequestModuleStatics::GetWebServerPort()
{
	return UWebRequestModule::Get().GetServerPort();
}

void UWebRequestModuleStatics::SetWebServerPort(int32 InServerPort)
{
	UWebRequestModule::Get().SetServerPort(InServerPort);
}

bool UWebRequestModuleStatics::HasWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().HasWebInterface(InClass);
}

UWebInterfaceBase* UWebRequestModuleStatics::GetWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().GetWebInterface(InClass);
}

UWebInterfaceBase* UWebRequestModuleStatics::CreateWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().CreateWebInterface(InClass);
}

bool UWebRequestModuleStatics::RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	return UWebRequestModule::Get().RegisterWebInterface(InClass, InOnRequestComplete);
}

bool UWebRequestModuleStatics::UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete)
{
	return UWebRequestModule::Get().UnRegisterWebInterface(InClass, InOnRequestComplete);
}

bool UWebRequestModuleStatics::UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().UnRegisterAllWebInterface(InClass);
}

bool UWebRequestModuleStatics::ClearWebInterface(TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().ClearWebInterface(InClass);
}

void UWebRequestModuleStatics::ClearAllWebInterface()
{
	UWebRequestModule::Get().ClearAllWebInterface();
}

bool UWebRequestModuleStatics::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModuleStatics::SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModuleStatics::K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().K2_SendWebRequest(InClass, InMethod, InParams, InHeadMap, InContent);
}
