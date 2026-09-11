// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetJson.h"

#include "Common/CommonModuleStatics.h"
#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetJson::UWebRequestHandle_GetJson()
{
	Json = nullptr;
}

void UWebRequestHandle_GetJson::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

	Json = nullptr;
}

void UWebRequestHandle_GetJson::Parse(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
	if(InResult.bSucceeded)
	{
		if(UCommonModuleStatics::StringToJsonObject(InResult.HttpResponse->GetContentAsString(), Json))
		{
			bSucceeded = true;
		}
	}
}
