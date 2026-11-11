// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModuleStatics.h"

#include "WebRequest/WebRequestModule.h"

bool UWebRequestModuleStatics::IsWebLocalMode(const UObject* WorldContextObject)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->IsLocalMode() : false;
}

void UWebRequestModuleStatics::SetWebLocalMode(const UObject* WorldContextObject, bool bInLocalMode)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Module) Module->SetLocalMode(bInLocalMode);
}

FString UWebRequestModuleStatics::GetWebServerURL(const UObject* WorldContextObject)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetServerURL() : FString();
}

void UWebRequestModuleStatics::SetWebServerURL(const UObject* WorldContextObject, const FString& InServerURL)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Module) Module->SetServerURL(InServerURL);
}

int32 UWebRequestModuleStatics::GetWebServerPort(const UObject* WorldContextObject)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetServerPort() : 0;
}

void UWebRequestModuleStatics::SetWebServerPort(const UObject* WorldContextObject, int32 InServerPort)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Module) Module->SetServerPort(InServerPort);
}

bool UWebRequestModuleStatics::HasWebInterface(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->HasWebInterface(InName) : false;
}

UWebInterfaceBase* UWebRequestModuleStatics::GetWebInterface(const UObject* WorldContextObject, const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetWebInterface(InName, InClass) : nullptr;
}

UWebInterfaceBase* UWebRequestModuleStatics::CreateWebInterface(const UObject* WorldContextObject, const FName InName, TSubclassOf<UWebInterfaceBase> InClass)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->CreateWebInterface(InName, InClass) : nullptr;
}

bool UWebRequestModuleStatics::RegisterWebInterface(const UObject* WorldContextObject, const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->RegisterWebInterface(InName, InOnRequestComplete) : false;
}

bool UWebRequestModuleStatics::UnRegisterWebInterface(const UObject* WorldContextObject, const FName InName, const FOnWebRequestComplete& InOnRequestComplete)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->UnRegisterWebInterface(InName, InOnRequestComplete) : false;
}

bool UWebRequestModuleStatics::UnRegisterAllWebInterface(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->UnRegisterAllWebInterface(InName) : false;
}

bool UWebRequestModuleStatics::ClearWebInterface(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->ClearWebInterface(InName) : false;
}

void UWebRequestModuleStatics::ClearAllWebInterface(const UObject* WorldContextObject)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Module) Module->ClearAllWebInterface();
}

FWebRequestConfig UWebRequestModuleStatics::GetWebRequestConfig(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetWebRequestConfig(InName) : FWebRequestConfig();
}

FWebRequestConfig UWebRequestModuleStatics::CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime) : FWebRequestConfig();
}

FWebRequestConfig UWebRequestModuleStatics::CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime) : FWebRequestConfig();
}

FWebRequestConfig UWebRequestModuleStatics::K2_CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->CreateWebRequestConfig(InName, InMethod, InParams, InHeadMap, InContent, InTriggerType, InTriggerTime) : FWebRequestConfig();
}

bool UWebRequestModuleStatics::ClearWebRequestConfig(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->ClearWebRequestConfig(InName) : false;
}

void UWebRequestModuleStatics::ClearAllWebRequestConfig(const UObject* WorldContextObject)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Module) Module->ClearAllWebRequestConfig();
}

bool UWebRequestModuleStatics::SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent) : false;
}

bool UWebRequestModuleStatics::SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent) : false;
}

bool UWebRequestModuleStatics::K2_SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->K2_SendWebRequest(InName, InMethod, InParams, InHeadMap, InContent) : false;
}

bool UWebRequestModuleStatics::SendWebRequestByConfig(const UObject* WorldContextObject, const FName InName)
{
	UWebRequestModule* Module = UWebRequestModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->SendWebRequestByConfig(InName) : false;
}
