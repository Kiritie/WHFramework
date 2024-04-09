// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/FileDownloader/FileDownloadManager.h"

#include "Main/MainManager.h"
#include "WebRequest/FileDownloader/FileDownloader.h"

const FUniqueType FFileDownloadManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FFileDownloadManager)

// Sets default values
FFileDownloadManager::FFileDownloadManager()
{

}

FFileDownloadManager::~FFileDownloadManager()
{
	
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
