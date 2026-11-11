// Fill out your copyright notice in the Description page of Project Settings.

#include "WebRequest/FileDownloader/FileDownloaderCollection.h"
#include "Type/NativeTypeIdentity.h"

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
	if (bDestroyed || DownloadResult != EDownloadToStorageResult::None) return;
	
	TArray<FWebFileURL> URLs = GetFileURLs();
	
	if (Downloaders.Num() > 0)
	{
		WHLog(TEXT("The downloader collection has started"), EDC_WebRequest, EDV_Warning);
		return;
	}

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
			const FString HashValue = LexToString(FileHash).Replace(TEXT("-"), TEXT(""));
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
			// AssetDownloader->bAsyncSave = true;

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

	if (Downloaders.IsEmpty())
	{
		OnComplete_Internal(EDownloadToStorageResult::Success);
		if (bAutoDestroy) DestroyDownload();
		return;
	}
	const TWeakPtr<FFileDownloaderCollection> WeakThis(SharedThis(this));
	const TSharedRef<TArray<TWeakPtr<FFileDownloader>>> Batch = MakeShared<TArray<TWeakPtr<FFileDownloader>>>();
	for (const TSharedPtr<FFileDownloader>& Item : Downloaders) Batch->Add(Item);
	for (int32 Index = 0; Index < Batch->Num(); ++Index)
	{
		TSharedPtr<FFileDownloader> Child = (*Batch)[Index].Pin();
		if (!Child) continue;
		const TWeakPtr<FFileDownloader> WeakChild = Child;
		Child->OnProgress.AddLambda([WeakThis, Index, Batch](FString CurrentFileUrl, int64, int64, int64 BytesSent, int64 BytesReceived, int64 FullSize)
		{
			TSharedPtr<FFileDownloaderCollection> Owner = WeakThis.Pin();
			if (!Owner || Owner->bDestroyed || Owner->DownloadResult != EDownloadToStorageResult::None) return;
			Owner->OnProgress_Internal(CurrentFileUrl, Index, Batch->Num(), BytesSent, BytesReceived, FullSize);
			Owner->OnProgress.Broadcast(CurrentFileUrl, Index, Batch->Num(), BytesSent, BytesReceived, FullSize);
		});
		Child->OnDestroy.AddLambda([WeakThis, WeakChild]()
		{
			TSharedPtr<FFileDownloaderCollection> Owner = WeakThis.Pin();
			TSharedPtr<FFileDownloader> ChildPtr = WeakChild.Pin();
			if (!Owner || Owner->bDestroyed) return;
			if (ChildPtr) Owner->RemoveDownloader(ChildPtr);
			if (Owner->DownloadResult == EDownloadToStorageResult::None)
				Owner->CancelDownload();
		});
		Child->OnComplete.AddLambda([WeakThis, Index, Batch, Timeout, ContentType, bForceByPayload, DownloadFilePaths](EDownloadToStorageResult Result, const FString&)
		{
			TSharedPtr<FFileDownloaderCollection> Owner = WeakThis.Pin();
			if (!Owner || Owner->bDestroyed || Owner->DownloadResult != EDownloadToStorageResult::None) return;
			const bool bSuccess = Result == EDownloadToStorageResult::Success || Result == EDownloadToStorageResult::SucceededByPayload;
			if (!bSuccess || Index + 1 == Batch->Num())
			{
				Owner->OnComplete_Internal(Result);
				if (Owner->bAutoDestroy) Owner->DestroyDownload();
				return;
			}
			TSharedPtr<FFileDownloader> Next = (*Batch)[Index + 1].Pin();
			if (!Next || !DownloadFilePaths.IsValidIndex(Index + 1))
			{
				Owner->OnComplete_Internal(EDownloadToStorageResult::DownloadFailed);
				if (Owner->bAutoDestroy) Owner->DestroyDownload();
				return;
			}
			Next->DownloadFile(DownloadFilePaths[Index + 1], Timeout, ContentType, bForceByPayload);
		});
	}
	if (TSharedPtr<FFileDownloader> First = (*Batch)[0].Pin())
		First->DownloadFile(DownloadFilePaths[0], Timeout, ContentType, bForceByPayload);
	else OnComplete_Internal(EDownloadToStorageResult::DownloadFailed);
}

void FFileDownloaderCollection::RefreshDownload(float DeltaSeconds)
{
	IFileDownloaderInterface::RefreshDownload(DeltaSeconds);
}

void FFileDownloaderCollection::CancelDownload()
{
	if (bDestroyed || DownloadResult != EDownloadToStorageResult::None) return;
	const TSharedPtr<FFileDownloaderCollection> KeepAlive = SharedThis(this);
	const TArray<TSharedPtr<FFileDownloader>> Active = Downloaders;
	OnComplete_Internal(EDownloadToStorageResult::Cancelled);
	for (const TSharedPtr<FFileDownloader>& Child : Active) if (Child) Child->CancelDownload();
	if (bAutoDestroy && !bDestroyed) DestroyDownload();
}

void FFileDownloaderCollection::DestroyDownload()
{
	if (bDestroyed) return;
	const TSharedPtr<FFileDownloaderCollection> KeepAlive = SharedThis(this);
	bDestroyed = true;
	if (DownloadResult == EDownloadToStorageResult::None) DownloadResult = EDownloadToStorageResult::Cancelled;
	const TArray<TSharedPtr<FFileDownloader>> Active = MoveTemp(Downloaders);
	Downloaders.Reset();
	for (const TSharedPtr<FFileDownloader>& Child : Active)
	{
		if (!Child) continue;
		Child->OnProgress.Clear();
		Child->OnComplete.Clear();
		Child->OnDestroy.Clear();
		Child->DestroyDownload();
	}
	OnDestroy_Internal();
	OnDestroy.Broadcast();
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

	if (FWebRequestManager* Manager = FWebRequestManager::GetPtr()) Manager->RemoveDownloader(SharedThis(this));
}
