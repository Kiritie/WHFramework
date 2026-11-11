// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Interface/Base/WebInterfaceBase.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "WebRequest/Handle/Base/WebRequestHandleBase.h"
#include "WebRequest/WebRequestModule.h"
#include "Engine/World.h"

UWebInterfaceBase::UWebInterfaceBase()
{
	Name = NAME_None;
	Url = TEXT("");
	HandleClass = UWebRequestHandleBase::StaticClass();
}

void UWebInterfaceBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWebInterfaceBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OnWebRequestComplete.Clear();
	auto PreviousHandles = MoveTemp(Handles);
	Handles.Empty();
	for(UWebRequestHandleBase* Handle : PreviousHandles)
	{
		UObjectPoolModuleStatics::DespawnObject(this, Handle);
	}
}

void UWebInterfaceBase::OnRequestComplete_Implementation(UWebRequestHandleBase* InWebRequestHandle)
{
	
}

void UWebInterfaceBase::RequestComplete(FWebRequestResult InResult, const TArray<FParameter>& InParams)
{
    UWebRequestHandleBase* Handle = UObjectPoolModuleStatics::SpawnObject<UWebRequestHandleBase>(this, HandleClass);
    if (!Handle) return;
    Handles.Add(Handle);
    Handle->Parse(InResult, InParams);
    OnRequestComplete(Handle);
    OnWebRequestComplete.Broadcast(Handle);
}

FString UWebInterfaceBase::GetFullUrl() const
{
	if (!FullUrl.IsEmpty()) return FullUrl;
	if (Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)) return Url;
	const UWebRequestModule* Owner = UWebRequestModule::GetPtr(GetWorld());
	const FString Server = Owner ? Owner->GetServerURL() : FString();
	return Server.IsEmpty() ? FString() : Server + Url;
}
