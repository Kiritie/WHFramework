// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WHFrameworkCoreTypes.h"
#include "WebRequest/WebRequestManager.h"
#include "WebRequest/WebRequestTypes.h"

class FFileDownloader;

class WHFRAMEWORKCORE_API IFileDownloaderInterface : public FUniqueClass, public TSharedFromThis<IFileDownloaderInterface>
{
public:	
	// ParamSets default values for this actor's properties
	IFileDownloaderInterface(FUniqueType InType);
	virtual ~IFileDownloaderInterface() override;

	static const FUniqueType Type;

public:
	/**
	 * Create a downloader
	 */
	template <typename DownloaderType, typename DownloaderData>
	static TSharedPtr<DownloaderType> CreateDownloader(const DownloaderData& Data)
	{
		TSharedPtr<DownloaderType> Downloader = MakeShareable(new DownloaderType(Data));
		FWebRequestManager::Get().AddDownloader(Downloader);
		return Downloader;
	}

	/**
	 * Refresh the download
	 */
	virtual void RefreshDownload(float DeltaSeconds);

	/**
	 * Cancel the download
	 */
	virtual void CancelDownload();

	/**
	 * Destroy the download
	 */
	virtual void DestroyDownload();

protected:
	virtual void OnProgress_Internal(FString CurrentFileUrl, int64 CurrentFileIndex, int64 TotalFileNum, int64 BytesSent, int64 BytesReceived, int64 FullSize);

public:
	FOnFileDownloadProgress OnProgress;
	
	FOnFileDownloadComplete OnComplete;

	FOnFileDownloadDestroy OnDestroy;

	bool bAsyncSave;

	bool bAutoDestroy;
	
	int64 FileBytesSent;
	
	int64 FileBytesReceived;
	
	int64 FileFullSize;
	
	FString FileSavePath;

	FString FilePath;
	FString FileName;
	FString FileExtension;

	EDownloadToStorageResult DownloadResult;
};
