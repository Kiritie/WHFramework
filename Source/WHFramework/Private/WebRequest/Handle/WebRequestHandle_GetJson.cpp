// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetJson.h"

#include "Global/GlobalBPLibrary.h"
#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetJson::UWebRequestHandle_GetJson()
{
	Json = nullptr;
}

void UWebRequestHandle_GetJson::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Json = nullptr;
}

void UWebRequestHandle_GetJson::Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
	if(InResult.bSucceeded)
	{
		if(UGlobalBPLibrary::ParseJsonObjectFromString(InResult.HttpResponse->GetContentAsString(), Json))
		{
			bSucceeded = true;
		}
	}
}
