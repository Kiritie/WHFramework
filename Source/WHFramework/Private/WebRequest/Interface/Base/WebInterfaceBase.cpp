// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Interface/Base/WebInterfaceBase.h"

#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UWebInterfaceBase::UWebInterfaceBase()
{
	Name = NAME_None;
	Url = TEXT("");
	Handle = UWebRequestHandleBase::StaticClass();
}

void UWebInterfaceBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UWebInterfaceBase::OnDespawn_Implementation()
{
	Name = NAME_None;
	Url = TEXT("");
	Handle = nullptr;
	OnWebRequestComplete.Clear();
}

void UWebInterfaceBase::OnRequestComplete_Implementation(FWebRequestInfo InWebRequestInfo)
{
	if(UWebRequestHandleBase* WebRequestHandle = UObjectPoolModuleBPLibrary::SpawnObject<UWebRequestHandleBase>(nullptr, Handle))
	{
		WebRequestHandle->Fill(InWebRequestInfo);
		OnWebRequestComplete.Broadcast(WebRequestHandle);
		UObjectPoolModuleBPLibrary::DespawnObject(WebRequestHandle);
	}
}
