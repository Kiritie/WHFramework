// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Interface/Base/WebInterfaceBase.h"

#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "WebRequest/Handle/Base/WebRequestHandleBase.h"

UWebInterfaceBase::UWebInterfaceBase()
{
	Name = NAME_None;
	Url = TEXT("");
	HandleClass = UWebRequestHandleBase::StaticClass();
	HandleMap = TMap<FString, UWebRequestHandleBase*>();
}

void UWebInterfaceBase::OnDespawn_Implementation()
{
	OnWebRequestComplete.Clear();
	for(auto Iter : HandleMap)
	{
		 UObjectPoolModuleBPLibrary::DespawnObject(Iter.Value);
	}
	HandleMap.Empty();
}

void UWebInterfaceBase::OnRequestComplete_Implementation(UWebRequestHandleBase* InWebRequestHandle)
{
	
}

void UWebInterfaceBase::RequestComplete(FWebRequestResult InWebRequestResult)
{
	UWebRequestHandleBase* WebRequestHandle = nullptr;
	if(!HandleMap.Contains(InWebRequestResult.Content))
	{
		WebRequestHandle = UObjectPoolModuleBPLibrary::SpawnObject<UWebRequestHandleBase>(nullptr, HandleClass);
		HandleMap.Add(InWebRequestResult.Content, WebRequestHandle);
	}
	else
	{
		WebRequestHandle = HandleMap[InWebRequestResult.Content];
	}
	if(WebRequestHandle)
	{
		if(InWebRequestResult.bSucceeded)
		{
			WebRequestHandle->Fill(InWebRequestResult);
		}
		OnRequestComplete(WebRequestHandle);
		HandleMap.Emplace(InWebRequestResult.Content, WebRequestHandle);
		OnWebRequestComplete.Broadcast(WebRequestHandle);
	}
}
