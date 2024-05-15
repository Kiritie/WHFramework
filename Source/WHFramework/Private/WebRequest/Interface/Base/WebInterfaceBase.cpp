// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Interface/Base/WebInterfaceBase.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "WebRequest/Handle/Base/WebRequestHandleBase.h"

UWebInterfaceBase::UWebInterfaceBase()
{
	Name = NAME_None;
	Url = TEXT("");
	HandleClass = UWebRequestHandleBase::StaticClass();
	HandleMap = TMap<FString, UWebRequestHandleBase*>();
}

void UWebInterfaceBase::OnDespawn_Implementation(bool bRecovery)
{
	OnWebRequestComplete.Clear();
	for(auto& Iter : HandleMap)
	{
		 UObjectPoolModuleStatics::DespawnObject(Iter.Value);
	}
	HandleMap.Empty();
}

void UWebInterfaceBase::OnRequestComplete_Implementation(UWebRequestHandleBase* InWebRequestHandle)
{
	
}

void UWebInterfaceBase::RequestComplete(FWebRequestResult InResult, const TArray<FParameter>& InParams)
{
	UWebRequestHandleBase* WebRequestHandle;
	if(!HandleMap.Contains(InResult.Content))
	{
		WebRequestHandle = UObjectPoolModuleStatics::SpawnObject<UWebRequestHandleBase>(nullptr, nullptr, false, HandleClass);
		HandleMap.Add(InResult.Content, WebRequestHandle);
	}
	else
	{
		WebRequestHandle = HandleMap[InResult.Content];
	}
	if(WebRequestHandle)
	{
		if(InResult.bSucceeded)
		{
			WebRequestHandle->Parse(InResult, InParams);
		}
		OnRequestComplete(WebRequestHandle);
		OnWebRequestComplete.Broadcast(WebRequestHandle);
		HandleMap.Emplace(InResult.Content, WebRequestHandle);
	}
}

FString UWebInterfaceBase::GetFullUrl() const
{
	return FullUrl.IsEmpty() ? UWebRequestModuleStatics::GetWebServerURL() + Url : FullUrl;
}
