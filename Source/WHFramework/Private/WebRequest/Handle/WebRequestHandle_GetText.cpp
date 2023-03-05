// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetText.h"

#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetText::UWebRequestHandle_GetText()
{
	Text = TEXT("");
}

void UWebRequestHandle_GetText::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Text = TEXT("");
}

void UWebRequestHandle_GetText::Fill(const FWebRequestResult& InWebRequestResult)
{
	Super::Fill(InWebRequestResult);
	
	Text = InWebRequestResult.HttpResponse->GetContentAsString();
}
