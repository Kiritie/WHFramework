// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/Base/WebRequestHandleBase.h"

UWebRequestHandleBase::UWebRequestHandleBase()
{
	bSucceeded = false;
}

void UWebRequestHandleBase::OnDespawn_Implementation(bool bRecovery)
{
	bSucceeded = false;
}

void UWebRequestHandleBase::Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
	bSucceeded = InResult.bSucceeded;
}
