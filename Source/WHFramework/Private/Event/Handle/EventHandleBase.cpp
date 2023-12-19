// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/EventHandleBase.h"

UEventHandleBase::UEventHandleBase()
{
	EventType = EEventType::Multicast;
}

bool UEventHandleBase::Filter_Implementation(UObject* InOwner, const FName InFuncName) const
{
	return true;
}

void UEventHandleBase::Parse_Implementation(const TArray<FParameter>& InParams)
{
	
}

TArray<FParameter> UEventHandleBase::Pack_Implementation()
{
	return TArray<FParameter>();
}

void UEventHandleBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
}

void UEventHandleBase::OnDespawn_Implementation(bool bRecovery)
{

}
