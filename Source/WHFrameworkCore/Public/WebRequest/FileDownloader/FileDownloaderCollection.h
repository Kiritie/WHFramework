// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "FileDownloaderInterface.h"

class FFileDownloader;

class WHFRAMEWORKCORE_API FFileDownloaderCollection : public IFileDownloaderInterface
{
public:	
	// ParamSets default values for this actor's properties
	FFileDownloaderCollection(FUniqueType InType, const TArray<FWebFileURL>& InURLs);
	FFileDownloaderCollection(const TArray<FWebFileURL>& InURLs);
	virtual ~FFileDownloaderCollection() override;

	static const FUniqueType Type;

public:
	/**
	 * Download the file and save it to the device disk
	 *
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param bForceByPayload If true, the file will be downloaded by payload even if the Content-Length header is present in the response
	 */
	virtual void DownloadFiles(const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload);

	virtual void RefreshDownload(float DeltaSeconds) override;

	virtual void CancelDownload() override;

	virtual void DestroyDownload() override;

	virtual void AddDownloader(const TSharedPtr<FFileDownloader>& Downloader);

	virtual void RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader);

protected:
	virtual void OnComplete_Internal(EDownloadToStorageResult Result);

	virtual void OnDestroy_Internal();

public:
	TArray<FWebFileURL> FileURLs;
	
	TArray<FString> IgnoreFiles;

	TArray<TSharedPtr<FFileDownloader>> Downloaders;

public:
	TArray<TSharedPtr<FFileDownloader>> GetAllDownloader() { return Downloaders; }

	virtual TArray<FWebFileURL> GetFileURLs() const { return FileURLs; }
};
