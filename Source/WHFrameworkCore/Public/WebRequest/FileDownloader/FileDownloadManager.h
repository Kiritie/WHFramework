// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class FFileDownloader;

class WHFRAMEWORKCORE_API FFileDownloadManager : public FManagerBase
{
	GENERATED_MANAGER(FFileDownloadManager)

public:	
	// ParamSets default values for this actor's properties
	FFileDownloadManager();

	virtual ~FFileDownloadManager() override;
	
	static const FUniqueType Type;

public:
	void AddDownloader(const TSharedPtr<FFileDownloader>& Downloader);

	void RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader);

protected:
	TArray<TSharedPtr<FFileDownloader>> DownloaderList;

public:
	TArray<TSharedPtr<FFileDownloader>> GetDownloaderList() { return DownloaderList; }
};
