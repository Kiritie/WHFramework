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

bool UWebRequestModuleStatics::HasWebInterface(const FName InName)
{
	return UWebRequestModule::Get().HasWebInterface(InName);
}

UWebInterfaceBase* UWebRequestModuleStatics::GetWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().GetWebInterface(InName, InClass);
}

UWebInterfaceBase* UWebRequestModuleStatics::CreateWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	return UWebRequestModule::Get().CreateWebInterface(InName, InClass);
}

bool UWebRequestModuleStatics::RegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	return UWebRequestModule::Get().RegisterWebInterface(InName, InOnRequestComplete);
}

bool UWebRequestModuleStatics::UnRegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	return UWebRequestModule::Get().UnRegisterWebInterface(InName, InOnRequestComplete);
}

bool UWebRequestModuleStatics::UnRegisterAllWebInterface(const FName InName)
{
	return UWebRequestModule::Get().UnRegisterAllWebInterface(InName);
}

bool UWebRequestModuleStatics::ClearWebInterface(const FName InName)
{
	return UWebRequestModule::Get().ClearWebInterface(InName);
}

void UWebRequestModuleStatics::ClearAllWebInterface()
{
	UWebRequestModule::Get().ClearAllWebInterface();
}

FWebRequestConfig UWebRequestModuleStatics::GetWebRequestConfig(const FName InName)
{
	return UWebRequestModule::Get().GetWebRequestConfig(InName);
}

FWebRequestConfig UWebRequestModuleStatics::CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return UWebRequestModule::Get().CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime);
}

FWebRequestConfig UWebRequestModuleStatics::CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return UWebRequestModule::Get().CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime);
}

FWebRequestConfig UWebRequestModuleStatics::K2_CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	return UWebRequestModule::Get().CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime);
}

bool UWebRequestModuleStatics::ClearWebRequestConfig(const FName InName)
{
	return UWebRequestModule::Get().ClearWebRequestConfig(InName);
}

void UWebRequestModuleStatics::ClearAllWebRequestConfig()
{
	UWebRequestModule::Get().ClearAllWebRequestConfig();
}

bool UWebRequestModuleStatics::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModuleStatics::SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModuleStatics::K2_SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	return UWebRequestModule::Get().K2_SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent);
}

bool UWebRequestModuleStatics::SendWebRequestByConfig(const FName InName)
{
	return UWebRequestModule::Get().SendWebRequestByConfig(InName);
}
