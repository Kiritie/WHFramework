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
	virtual void OnInitialize() override;

public:
	virtual void AddDownloader(const TSharedPtr<FFileDownloader>& Downloader);

	virtual void RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader);

protected:
	TArray<TSharedPtr<FFileDownloader>> DownloaderList;

public:
	TArray<TSharedPtr<FFileDownloader>> GetAllDownloader() { return DownloaderList; }

	template<class T>
	TArray<TSharedPtr<T>> GetAllDownloader()
	{
		TArray<TSharedPtr<T>> ReturnValues;
		for(auto Iter : FFileDownloadManager::Get().GetAllDownloader())
		{
			if(TSharedPtr<T> Downloader = StaticCastSharedPtr<T>(Iter))
			{
				ReturnValues.Add(Downloader);
			}
		}
		return ReturnValues;
	}
};
