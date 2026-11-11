// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetText.h"

#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetText::UWebRequestHandle_GetText()
{
	Text = TEXT("");
}

void UWebRequestHandle_GetText::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

	Text = TEXT("");
}

void UWebRequestHandle_GetText::Parse(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
	bSucceeded = false;
	Text.Reset();
	if(InResult.bSucceeded && InResult.HttpResponse.IsValid())
	{
		Text = InResult.HttpResponse->GetContentAsString();
		bSucceeded = true;
	}
}
