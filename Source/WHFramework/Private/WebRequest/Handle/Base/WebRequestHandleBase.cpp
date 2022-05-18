// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/Base/WebRequestHandleBase.h"

UWebRequestHandleBase::UWebRequestHandleBase()
{
	bSucceeded = false;
}

void UWebRequestHandleBase::OnDespawn_Implementation()
{
	bSucceeded = false;
}

void UWebRequestHandleBase::Fill(const FWebRequestResult& InWebRequestResult)
{
	bSucceeded = InWebRequestResult.bSucceeded;
}
