// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetJson.h"

#include "Global/GlobalBPLibrary.h"
#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetJson::UWebRequestHandle_GetJson()
{
	Json = nullptr;
}

void UWebRequestHandle_GetJson::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	Json = nullptr;
}

void UWebRequestHandle_GetJson::Fill(const FWebRequestResult& InWebRequestResult)
{
	Super::Fill(InWebRequestResult);

	UGlobalBPLibrary::ParseJsonObjectFromString(InWebRequestResult.HttpResponse->GetContentAsString(), Json);
}
