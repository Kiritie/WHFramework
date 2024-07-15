// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/FileDownloader/FileDownloaderCollection.h"

#include "Asset/AssetTypes.h"
#include "Debug/DebugTypes.h"
#include "WebRequest/FileDownloader/FileDownloader.h"
#include "WebRequest/WebRequestManager.h"

const FUniqueType FFileDownloaderCollection::Type = FUniqueType(&IFileDownloaderInterface::Type);

FFileDownloaderCollection::FFileDownloaderCollection(FUniqueType InType, const TArray<FWebFileURL>& InURLs) : IFileDownloaderInterface(InType)
{
	FileURLs = InURLs;
}

// Sets default values
FFileDownloaderCollection::FFileDownloaderCollection(const TArray<FWebFileURL>& InURLs) : IFileDownloaderInterface(Type)
{
	FileURLs = InURLs;
}

FFileDownloaderCollection::~FFileDownloaderCollection()
{
	
}

void FFileDownloaderCollection::DownloadFiles(const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload)
{
	if(DownloadResult == EDownloadToStorageResult::Cancelled) return;
	
	TArray<FWebFileURL> URLs = GetFileURLs();
	
	if (URLs.IsEmpty())
	{
		WHLog(FString::Printf(TEXT("You have not provided an URL to download the file")), EDC_WebRequest, EDV_Error);
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::InvalidURL, SavePath);
		return;
	}

	if (SavePath.IsEmpty())
	{
		WHLog(FString::Printf(TEXT("You have not provided a path to save the file")), EDC_WebRequest, EDV_Error);
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::InvalidSavePath, SavePath);
		return;
	}

	if (Timeout < 0)
	{
		WHLog(FString::Printf(TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout), EDC_WebRequest, EDV_Warning);
		Timeout = 0;
	}

	FileSavePath = SavePath;

	FPaths::Split(FileSavePath, FilePath, FileName, FileExtension);

	TArray<FString> AllFilePaths, DownloadFilePaths;

	IFileManager& FileManager = IFileManager::Get();

	for(auto& Iter : URLs)
	{
		const FString _FilePath = FileSavePath + Iter.FilePath;
		
		AllFilePaths.Add(_FilePath);

		bool bNeedDownload = true;
		if(FileManager.FileExists(*_FilePath))
		{
			const FMD5Hash FileHash = FMD5Hash::HashFile(*_FilePath);
			const FString HashValue = LexToString(FileHash);
			if(HashValue.Equals(Iter.MD5Hash))
			{
				bNeedDownload = false;
			}
		}

		if(bNeedDownload)
		{
			WHLog(FString::Printf(TEXT("Download the collection to find different file: %s, And will to upgrade it."), *_FilePath), EDC_WebRequest);
			
			DownloadFilePaths.Add(_FilePath);

			TSharedPtr<FFileDownloader> AssetDownloader = CreateDownloader<FFileDownloader>(Iter);

			AddDownloader(AssetDownloader);
		}
	}

	FSimpleFileVisitor DirectoryVisitor;
	FileManager.IterateDirectoryRecursively(*FileSavePath, DirectoryVisitor);
	for(auto& Iter : DirectoryVisitor.Files)
	{
		if(!AllFilePaths.Contains(Iter) && !IgnoreFiles.Contains(FPaths::GetCleanFilename(Iter)))
		{
			WHLog(FString::Printf(TEXT("Download the collection to find unlink file: %s, And will to delete it."), *Iter), EDC_WebRequest);

			FileManager.Delete(*Iter);
		}
	}

	for(int32 i = 0; i < Downloaders.Num(); i++)
	{
		TWeakPtr<FFileDownloaderCollection> _WeakThis(SharedThis(this));
		
		Downloaders[i]->OnProgress.AddLambda([_WeakThis, i](FString FileUrl, int64 BytesSent, int64 BytesReceived, int64 FullSize)
		{
			TSharedPtr<FFileDownloaderCollection> ThisPtr = _WeakThis.Pin();
			if(!ThisPtr.IsValid()) return;

			ThisPtr->OnProgress_Internal(0, i + 1, ThisPtr->Downloaders.Num());
			ThisPtr->OnProgress.Broadcast(FileUrl, 0, i + 1, ThisPtr->Downloaders.Num());
		});

		Downloaders[i]->OnDestroy.AddLambda([_WeakThis, i]()
		{
			TSharedPtr<FFileDownloaderCollection> ThisPtr = _WeakThis.Pin();
			if(!ThisPtr.IsValid()) return;

			ThisPtr->RemoveDownloader(ThisPtr->Downloaders[i]);

			if(ThisPtr->Downloaders.IsEmpty())
			{
				ThisPtr->OnDestroy_Internal();

				ThisPtr->OnDestroy.Broadcast();
			}
		});
		
		Downloaders[i]->OnComplete.AddLambda([_WeakThis, i, Timeout, ContentType, bForceByPayload, DownloadFilePaths](EDownloadToStorageResult Result, const FString& SavePath)
		{
			TSharedPtr<FFileDownloaderCollection> ThisPtr = _WeakThis.Pin();
			if(!ThisPtr.IsValid()) return;

			if(i < ThisPtr->Downloaders.Num() - 1 && Result != EDownloadToStorageResult::Cancelled)
			{
				ThisPtr->Downloaders[i + 1]->DownloadFile(DownloadFilePaths[i + 1], Timeout, ContentType, bForceByPayload);
			}
			else
			{
				ThisPtr->OnComplete_Internal(Result);

				if(ThisPtr->bAutoDestroy)
				{
					ThisPtr->OnDestroy_Internal();

					ThisPtr->OnDestroy.Broadcast();
				}
			}
		});
	}

	if(Downloaders.IsValidIndex(0))
	{
		Downloaders[0]->DownloadFile(DownloadFilePaths[0], Timeout, ContentType, bForceByPayload);
	}
}

void FFileDownloaderCollection::RefreshDownload(float DeltaSeconds)
{
	IFileDownloaderInterface::RefreshDownload(DeltaSeconds);
}

void FFileDownloaderCollection::CancelDownload()
{
	OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::Cancelled, FileSavePath);

	if(!Downloaders.IsEmpty())
	{
		for(auto& Iter : TArray(Downloaders))
		{
			Iter->CancelDownload();
		}
	}
	else if(bAutoDestroy)
	{
		OnDestroy_Internal();

		OnDestroy.Broadcast();
	}
}

void FFileDownloaderCollection::DestroyDownload()
{
	if(!Downloaders.IsEmpty())
	{
		for(auto& Iter : TArray(Downloaders))
		{
			Iter->DestroyDownload();
		}
	}
	else
	{
		OnDestroy_Internal();

		OnDestroy.Broadcast();
	}
}

void FFileDownloaderCollection::AddDownloader(const TSharedPtr<FFileDownloader>& Downloader)
{
	if(!Downloaders.Contains(Downloader))
	{
		Downloaders.Add(Downloader);
	}
}
 
void FFileDownloaderCollection::RemoveDownloader(const TSharedPtr<FFileDownloader>& Downloader)
{
	if(Downloaders.Contains(Downloader))
	{
		Downloaders.Remove(Downloader);
	}
}

void FFileDownloaderCollection::OnComplete_Internal(EDownloadToStorageResult Result)
{
	WHLog(FString::Printf(TEXT("Download collection completed")), EDC_WebRequest, EDV_Log);

	OnComplete.Broadcast(DownloadResult = Result, FileSavePath);
}

void FFileDownloaderCollection::OnDestroy_Internal()
{
	WHLog(FString::Printf(TEXT("Download collection destroyed")), EDC_WebRequest, EDV_Log);

	FWebRequestManager::Get().RemoveDownloader(SharedThis(this));
}
