﻿// Georgy Treshchev 2021.

#include "WebRequest/FileDownloader/FileDownloader.h"

#include "HttpModule.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IHttpResponse.h"
#include "WebRequest/FileDownloader/FileDownloadManager.h"

FFileDownloader::FFileDownloader()
{
	FileType = EDownloadFileType::Resource;
	DownloadResult = EDownloadToStorageResult::None;
}

FFileDownloader::~FFileDownloader()
{
}

TSharedPtr<FFileDownloader> FFileDownloader::Create()
{
	const auto Downloader = MakeShared<FFileDownloader>();
	FFileDownloadManager::Get().AddDownloader(Downloader);
	return Downloader;
}

void FFileDownloader::DownloadFile(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("You have not provided an URL to download the file"));
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::InvalidURL, SavePath);
		return;
	}

	if (SavePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("You have not provided a path to save the file"));
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::InvalidSavePath, SavePath);
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

	FileSavePath = SavePath;

	FPaths::Split(FileSavePath, FilePath, FileName, FileExtension);

	auto OnResult = [this](FFileDownloaderResult&& Result) mutable
	{
		OnComplete_Internal(Result.Result, MoveTemp(Result.Data));
	};

	if (bForceByPayload)
	{
		DownloadFileByPayload(URL, Timeout, ContentType).Next(OnResult);
	}
	else
	{
		DownloadFile(URL, Timeout, ContentType, TNumericLimits<TArray<uint8>::SizeType>::Max()).Next(OnResult);
	}
}

void FFileDownloader::OnComplete_Internal(EDownloadToMemoryResult Result, TArray64<uint8> DownloadedContent)
{
	if (Result != EDownloadToMemoryResult::Success && Result != EDownloadToMemoryResult::SucceededByPayload)
	{
		// TODO: redesign in a more elegant way
		switch (Result)
		{
		case EDownloadToMemoryResult::Cancelled:
			OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::Cancelled, FileSavePath);
			break;
		case EDownloadToMemoryResult::DownloadFailed:
			OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::DownloadFailed, FileSavePath);
			break;
		case EDownloadToMemoryResult::InvalidURL:
			OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::InvalidURL, FileSavePath);
			break;
		}
		return;
	}

	if (!DownloadedContent.IsValidIndex(0))
	{
		UE_LOG(LogTemp, Error, TEXT("An error occurred while downloading the file to storage"));
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::DownloadFailed, FileSavePath);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create save directory if it does not exist
	{
		FString Path, Filename, Extension;
		FPaths::Split(FileSavePath, Path, Filename, Extension);
		if (!PlatformFile.DirectoryExists(*Path))
		{
			if (!PlatformFile.CreateDirectoryTree(*Path))
			{
				UE_LOG(LogTemp, Error, TEXT("Unable to create a directory '%s' to save the downloaded file"), *Path);
				OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::DirectoryCreationFailed, FileSavePath);
				return;
			}
		}
	}

	// Delete the file if it already exists
	if (FPaths::FileExists(*FileSavePath))
	{
		IFileManager& FileManager = IFileManager::Get();
		if (!FileManager.Delete(*FileSavePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Something went wrong while deleting the existing file '%s'"), *FileSavePath);
			OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
			return;
		}
	}

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Something went wrong while saving the file '%s'"), *FileSavePath);
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
		return;
	}

	if (!FileHandle->Write(DownloadedContent.GetData(), DownloadedContent.Num()))
	{
		UE_LOG(LogTemp, Error, TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath);
		delete FileHandle;
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
		return;
	}

	delete FileHandle;
	OnComplete.Broadcast(DownloadResult = (Result == EDownloadToMemoryResult::SucceededByPayload ? EDownloadToStorageResult::SucceededByPayload : EDownloadToStorageResult::Success), FileSavePath);
}

TFuture<FFileDownloaderResult> FFileDownloader::DownloadFile(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	TWeakPtr<FFileDownloader> WeakThisPtr = AsShared();
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize](int64 ContentSize) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			UE_LOG(LogTemp, Warning, TEXT("Canceled file download from %s"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		auto DownloadByPayload = [SharedThis, WeakThisPtr, PromisePtr, URL, Timeout, ContentType]()
		{
			SharedThis->DownloadFileByPayload(URL, Timeout, ContentType).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType](FFileDownloaderResult Result) mutable
			{
				TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
				if (!SharedThis.IsValid())
					{
					UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
					PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
					return;
				}

				if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
				{
					UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
					PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
					return;
				}

				PromisePtr->SetValue(FFileDownloaderResult{Result.Result, MoveTemp(Result.Data)});
			});
		};
		
		if (ContentSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL);
			DownloadByPayload();
			return;
		}

		if (MaxChunkSize <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: MaxChunkSize is <= 0. Trying to download the file by payload"), *URL);
			DownloadByPayload();
			return;
		}

		TSharedPtr<TArray64<uint8>> OverallDownloadedDataPtr = MakeShared<TArray64<uint8>>();
		{
			UE_LOG(LogTemp, Warning, TEXT("Pre-allocating %lld bytes for file download from %s"), ContentSize, *URL);
			OverallDownloadedDataPtr->SetNumUninitialized(ContentSize);
		}

		FInt64Vector2 ChunkRange;
		{
			ChunkRange.X = 0;
			ChunkRange.Y = FMath::Min(MaxChunkSize, ContentSize) - 1;
		}

		TSharedPtr<int64> ChunkOffsetPtr = MakeShared<int64>(ChunkRange.X);
		TSharedPtr<bool> bChunkDownloadedFilledPtr = MakeShared<bool>(false);

		auto OnChunkDownloadedFilled = [bChunkDownloadedFilledPtr]()
		{
			if (bChunkDownloadedFilledPtr.IsValid())
			{
				*bChunkDownloadedFilledPtr = true;
			}
		};

		auto OnChunkDownloaded = [WeakThisPtr, PromisePtr, URL, ContentSize, Timeout, ContentType, DownloadByPayload, OverallDownloadedDataPtr, bChunkDownloadedFilledPtr, ChunkOffsetPtr, OnChunkDownloadedFilled](TArray64<uint8>&& ResultData) mutable
		{
			TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
			if (!SharedThis.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
				PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
			{
				UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
				PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			// Calculate the currently size of the downloaded content in the result buffer
			const int64 CurrentlyDownloadedSize = *ChunkOffsetPtr + ResultData.Num();

			// Check if some values are out of range
			{
				if (*ChunkOffsetPtr < 0 || *ChunkOffsetPtr >= OverallDownloadedDataPtr->Num())
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: data offset is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, *ChunkOffsetPtr, OverallDownloadedDataPtr->Num());
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}

				if (CurrentlyDownloadedSize > OverallDownloadedDataPtr->Num())
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: overall downloaded size is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, CurrentlyDownloadedSize, OverallDownloadedDataPtr->Num());
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}
			}

			// Append the downloaded chunk to the result data
			FMemory::Memcpy(OverallDownloadedDataPtr->GetData() + *ChunkOffsetPtr, ResultData.GetData(), ResultData.Num());

			// If the download is complete, return the result data
			if (*ChunkOffsetPtr + ResultData.Num() >= ContentSize)
			{
				PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Success, MoveTemp(*OverallDownloadedDataPtr.Get())});
				OnChunkDownloadedFilled();
				return;
			}

			// Increase the offset by the size of the downloaded chunk
			*ChunkOffsetPtr += ResultData.Num();
		};

		SharedThis->DownloadFilePerChunk(URL, Timeout, ContentType, MaxChunkSize, ChunkRange, OnChunkDownloaded).Next([PromisePtr, bChunkDownloadedFilledPtr, URL, OverallDownloadedDataPtr, OnChunkDownloadedFilled, DownloadByPayload](EDownloadToMemoryResult Result) mutable
		{
			// Only return data if no chunk was downloaded
			if (bChunkDownloadedFilledPtr.IsValid() && (*bChunkDownloadedFilledPtr.Get() == false))
			{
				if (Result != EDownloadToMemoryResult::Success && Result != EDownloadToMemoryResult::SucceededByPayload)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: download failed. Trying to download the file by payload"), *URL);
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}
				OverallDownloadedDataPtr->Shrink();
				PromisePtr->SetValue(FFileDownloaderResult{Result, MoveTemp(*OverallDownloadedDataPtr.Get())});
			}
		});
	});
	return PromisePtr->GetFuture();
}

TFuture<EDownloadToMemoryResult> FFileDownloader::DownloadFilePerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, FInt64Vector2 ChunkRange, const TFunction<void(TArray64<uint8>&&)>& OnChunkDownloaded)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
		return MakeFulfilledPromise<EDownloadToMemoryResult>(EDownloadToMemoryResult::Cancelled).GetFuture();
	}

	TSharedPtr<TPromise<EDownloadToMemoryResult>> PromisePtr = MakeShared<TPromise<EDownloadToMemoryResult>>();
	TWeakPtr<FFileDownloader> WeakThisPtr = AsShared();
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize, OnChunkDownloaded, ChunkRange](int64 ContentSize) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}
		
		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
			return;
		}

		if (ContentSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL);
			SharedThis->DownloadFileByPayload(URL, Timeout, ContentType).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnChunkDownloaded](FFileDownloaderResult Result) mutable
			{
				TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
				if (!SharedThis.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
					return;
				}

				if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
				{
					UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
					return;
				}

				if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result));
					PromisePtr->SetValue(Result.Result);
					return;
				}

				if (Result.Data.Num() <= 0)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: downloaded content is empty"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
					return;
				}

				PromisePtr->SetValue(Result.Result);
				OnChunkDownloaded(MoveTemp(Result.Data));
			});
			return;
		}

		if (MaxChunkSize <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: max chunk size is <= 0"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}

		// If the chunk range is not specified, determine the range based on the max chunk size and the content size
		if (ChunkRange.X == 0 && ChunkRange.Y == 0)
		{
			ChunkRange.Y = FMath::Min(MaxChunkSize, ContentSize) - 1;
		}

		if (ChunkRange.Y > ContentSize)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: chunk range is out of range (%lld, expected [0, %lld])"), *URL, ChunkRange.Y, ContentSize);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}

		auto OnProgressInternal = [WeakThisPtr, URL, ChunkRange](int64 BytesSent, int64 BytesReceived, int64 ContentSize) mutable
		{
			TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
			if (SharedThis.IsValid())
			{
				const float Progress = ContentSize <= 0 ? 0.0f : static_cast<float>(BytesReceived + ChunkRange.X) / ContentSize;
				UE_LOG(LogTemp, Log, TEXT("Downloaded %lld bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *URL, ChunkRange.X, ChunkRange.Y, ContentSize, Progress);
				SharedThis->OnProgress.Broadcast(BytesSent, BytesReceived + ChunkRange.X, ContentSize);
			}
		};

		SharedThis->DownloadFileByChunk(URL, Timeout, ContentType, ContentSize, ChunkRange, OnProgressInternal).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, ContentSize, MaxChunkSize, OnChunkDownloaded, ChunkRange](FFileDownloaderResult&& Result)
		{
			TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
			if (!SharedThis.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
				PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
				return;
			}

			if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
			{
				UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
				PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
				return;
			}

			if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result));
				PromisePtr->SetValue(Result.Result);
				return;
			}

			OnChunkDownloaded(MoveTemp(Result.Data));

			// Check if the download is complete
			if (ContentSize > ChunkRange.Y + 1)
			{
				const int64 ChunkStart = ChunkRange.Y + 1;
				const int64 ChunkEnd = FMath::Min(ChunkStart + MaxChunkSize, ContentSize) - 1;

				SharedThis->DownloadFilePerChunk(URL, Timeout, ContentType, MaxChunkSize, FInt64Vector2(ChunkStart, ChunkEnd), OnChunkDownloaded).Next([WeakThisPtr, PromisePtr](EDownloadToMemoryResult Result)
				{
					PromisePtr->SetValue(Result);
				});
			}
			else
			{
				PromisePtr->SetValue(EDownloadToMemoryResult::Success);
			}
		});
	});

	return PromisePtr->GetFuture();
}

TFuture<FFileDownloaderResult> FFileDownloader::DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, FInt64Vector2 ChunkRange, const TFunction<void(int64, int64, int64)>& ProgressFun)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.X < 0 || ChunkRange.Y <= 0 || ChunkRange.X > ChunkRange.Y)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is invalid"), *URL, ChunkRange.X, ChunkRange.Y);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.Y - ChunkRange.X + 1 > ContentSize)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is out of range (%lld)"), *URL, ChunkRange.X, ChunkRange.Y, ContentSize);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FFileDownloader> WeakThisPtr = AsShared();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogTemp, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	if (!ContentType.IsEmpty())
	{
		HttpRequestRef->SetHeader(TEXT("Content-Type"), ContentType);
	}

	const FString RangeHeaderValue = FString::Format(TEXT("bytes={0}-{1}"), {ChunkRange.X, ChunkRange.Y});
	HttpRequestRef->SetHeader(TEXT("Range"), RangeHeaderValue);

	HttpRequestRef->OnRequestProgress().BindLambda([WeakThisPtr, ContentSize, ChunkRange, ProgressFun](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const float Progress = ContentSize <= 0 ? 0.0f : static_cast<float>(BytesReceived) / ContentSize;
			UE_LOG(LogTemp, Log, TEXT("Downloaded %d bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentSize, Progress);
			ProgressFun(BytesSent, BytesReceived, ContentSize);
		}
	});

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL, ChunkRange](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			UE_LOG(LogTemp, Warning, TEXT("Canceled file chunk download from %s"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: request failed"), *Request->GetURL());
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: content length is 0"), *Request->GetURL());
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));

		if (ContentLength != ChunkRange.Y - ChunkRange.X + 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: content length (%lld) does not match the expected length (%lld)"), *Request->GetURL(), ContentLength, ChunkRange.Y - ChunkRange.X + 1);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("Successfully downloaded file chunk from %s. Range: {%lld; %lld}, Overall: %lld"), *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentLength);
		PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Success, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download file chunk from %s: request failed"), *URL);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<FFileDownloaderResult> FFileDownloader::DownloadFileByPayload(const FString& URL, float Timeout, const FString& ContentType)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FFileDownloader> WeakThisPtr = AsShared();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogTemp, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	HttpRequestRef->OnRequestProgress().BindLambda([WeakThisPtr](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const int64 ContentLength = Request->GetContentLength();
			const float Progress = ContentLength <= 0 ? 0.0f : static_cast<float>(BytesReceived) / ContentLength;
			UE_LOG(LogTemp, Log, TEXT("Downloaded %d bytes of file chunk from %s by payload. Overall: %lld, Progress: %f"), BytesReceived, *Request->GetURL(), static_cast<int64>(Request->GetContentLength()), Progress);
			SharedThis->OnProgress.Broadcast(BytesSent, BytesReceived, ContentLength);
		}
	});

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to download file from %s by payload: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			UE_LOG(LogTemp, Warning, TEXT("Canceled file download from %s by payload"), *URL);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file from %s by payload: request failed"), *Request->GetURL());
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to download file from %s by payload: content length is 0"), *Request->GetURL());
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("Successfully downloaded file from %s by payload. Overall: %lld"), *Request->GetURL(), static_cast<int64>(Response->GetContentLength()));
		return PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::SucceededByPayload, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download file from %s by payload: request failed"), *URL);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<int64> FFileDownloader::GetContentSize(const FString& URL, float Timeout)
{
	TSharedPtr<TPromise<int64>> PromisePtr = MakeShared<TPromise<int64>>();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("HEAD");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogTemp, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	HttpRequestRef->OnProcessRequestComplete().BindLambda([PromisePtr, URL](const FHttpRequestPtr& Request, const FHttpResponsePtr& Response, const bool bSucceeded)
	{
		if (!bSucceeded || !Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get size of file from %s: request failed"), *URL);
			PromisePtr->SetValue(0);
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));
		if (ContentLength <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get size of file from %s: content length is %lld, expected > 0"), *URL, ContentLength);
			PromisePtr->SetValue(0);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("Got size of file from %s: %lld"), *URL, ContentLength);
		PromisePtr->SetValue(ContentLength);
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get size of file from %s: request failed"), *URL);
		return MakeFulfilledPromise<int64>(0).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

void FFileDownloader::CancelDownload()
{
	OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::Cancelled, FileSavePath);
	if (HttpRequestPtr.IsValid())
	{
#if UE_VERSION_NEWER_THAN(4, 26, 0)
		const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpRequestPtr.Pin();
#else
		const TSharedPtr<IHttpRequest> HttpRequest = HttpRequestPtr.Pin();
#endif

		HttpRequest->CancelRequest();
	}
	UE_LOG(LogTemp, Warning, TEXT("Download canceled"));

	DestroyDownload();
}

void FFileDownloader::DestroyDownload()
{
	FFileDownloadManager::Get().RemoveDownloader(AsShared());
}
