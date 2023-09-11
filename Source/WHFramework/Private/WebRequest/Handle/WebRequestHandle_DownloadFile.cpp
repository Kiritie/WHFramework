// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_DownloadFile.h"

#include "Global/GlobalBPLibrary.h"
#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_DownloadFile::UWebRequestHandle_DownloadFile()
{
	FilePath = TEXT("");
}

void UWebRequestHandle_DownloadFile::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	FilePath = TEXT("");
}

void UWebRequestHandle_DownloadFile::Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
	if (InParams.IsValidIndex(0))
	{
		FilePath = InParams[0].GetStringValue();
	}
	else
	{
		FString LeftStr, RightStr;
		InResult.HttpResponse->GetURL().Split(TEXT("/"), &LeftStr, &RightStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		FilePath = FPaths::ProjectSavedDir() + RightStr;
	}

	if (InResult.bSucceeded && InResult.HttpResponse->GetContentLength() > 0)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (IFileHandle* FileHandle = PlatformFile.OpenWrite(*FilePath))
		{
			FileHandle->Write(InResult.HttpResponse->GetContent().GetData(), InResult.HttpResponse->GetContentLength());
			delete FileHandle;
			bSucceeded = true;
		}
	}
}