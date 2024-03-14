// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FileDownloader/FileDownloader.h"

class WHFRAMEWORKCORE_API FFileDownloadManager
{
public:	
	// ParamSets default values for this actor's properties
	FFileDownloadManager();

	~FFileDownloadManager();

public:
	static FFileDownloadManager& Get();

public:
	void AddDownloader(const TSharedPtr<FFileDownloader>& Downloader);

	void RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader);

protected:
	static FFileDownloadManager* Instance;
	
	TArray<TSharedPtr<FFileDownloader>> DownloaderList;

public:
	TArray<TSharedPtr<FFileDownloader>> GetDownloaderList() { return DownloaderList; }
};
