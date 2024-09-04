// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/FileDownloader/FileDownloaderInterface.h"


const FUniqueType IFileDownloaderInterface::Type = FUniqueType(&FUniqueClass::Type);

IFileDownloaderInterface::IFileDownloaderInterface(FUniqueType InType) : FUniqueClass(InType)
{
	bAsyncSave = false;
	bAutoDestroy = false;
	FileBytesSent = 0;
	FileBytesReceived = 0;
	FileFullSize = 0;

	DownloadResult = EDownloadToStorageResult::None;
}

IFileDownloaderInterface::~IFileDownloaderInterface()
{
	
}

void IFileDownloaderInterface::RefreshDownload(float DeltaSeconds)
{
}

void IFileDownloaderInterface::CancelDownload()
{
}

void IFileDownloaderInterface::DestroyDownload()
{
}

void IFileDownloaderInterface::OnProgress_Internal(FString CurrentFileUrl, int64 CurrentFileIndex, int64 TotalFileNum, int64 BytesSent, int64 BytesReceived, int64 FullSize)
{
	FileBytesSent = BytesSent;
	FileBytesReceived = BytesReceived;
	FileFullSize = FullSize;
}
