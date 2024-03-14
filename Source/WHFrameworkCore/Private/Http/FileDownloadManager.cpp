// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/FileDownloadManager.h"

FFileDownloadManager* FFileDownloadManager::Instance = nullptr;

// Sets default values
FFileDownloadManager::FFileDownloadManager()
{

}

FFileDownloadManager::~FFileDownloadManager()
{
	
}

FFileDownloadManager& FFileDownloadManager::Get()
{
	if(!Instance)
	{
		Instance = new FFileDownloadManager();
	}
	return *Instance;
}

void FFileDownloadManager::AddDownloader(const TSharedPtr<FFileDownloader>& Downloader)
{
	if(!DownloaderList.Contains(Downloader))
	{
		DownloaderList.Add(Downloader);
	}
}

void FFileDownloadManager::RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader)
{
	if(DownloaderList.Contains(Downloader))
	{
		DownloaderList.Remove(Downloader);
	}
}
