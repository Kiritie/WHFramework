// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Interface/Base/WebInterfaceBase.h"

#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "WebRequest/Handle/Base/WebRequestHandleBase.h"

UWebInterfaceBase::UWebInterfaceBase()
{
	Name = NAME_None;
	Url = TEXT("");
	Handle = UWebRequestHandleBase::StaticClass();
}

void UWebInterfaceBase::OnDespawn_Implementation()
{
	Name = NAME_None;
	Url = TEXT("");
	Handle = nullptr;
	OnWebRequestComplete.Clear();
}

void UWebInterfaceBase::OnRequestComplete_Implementation(FWebRequestResult InWebRequestResult)
{
	if(UWebRequestHandleBase* WebRequestHandle = UObjectPoolModuleBPLibrary::SpawnObject<UWebRequestHandleBase>(nullptr, Handle))
	{
		if(InWebRequestResult.bSucceeded)
		{
			WebRequestHandle->Fill(InWebRequestResult);
		}
		OnWebRequestComplete.Broadcast(WebRequestHandle);
		UObjectPoolModuleBPLibrary::DespawnObject(WebRequestHandle);
	}
}
