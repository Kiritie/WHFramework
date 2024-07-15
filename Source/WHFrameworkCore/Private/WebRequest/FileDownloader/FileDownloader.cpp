// Georgy Treshchev 2021.

#include "WebRequest/FileDownloader/FileDownloader.h"

#include "HttpModule.h"
#include "Debug/DebugTypes.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IHttpResponse.h"
#include "WebRequest/WebRequestManager.h"

const FUniqueType FFileDownloader::Type = FUniqueType(&IFileDownloaderInterface::Type);

FFileDownloader::FFileDownloader(FUniqueType InType, const FWebFileURL& InURL): IFileDownloaderInterface(InType)
{
	FileURL = InURL;
}

FFileDownloader::FFileDownloader(const FWebFileURL& InURL): IFileDownloaderInterface(Type)
{
	FileURL = InURL;
}

FFileDownloader::~FFileDownloader()
{
}

void FFileDownloader::DownloadFile(const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload)
{
	if(DownloadResult == EDownloadToStorageResult::Cancelled) return;

	FString URL = (GetFileURL().FileURL.StartsWith(TEXT("/")) ? FWebRequestManager::Get().GetServerURL() : TEXT("")) + GetFileURL().FileURL;

	if (URL.IsEmpty())
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

	auto OnResult = [this](FFileDownloaderResult&& Result) mutable
	{
		HttpRequestPtr.Reset();
		
		OnComplete_Internal(Result.Result, MoveTemp(Result.Data));

		OnComplete.Broadcast(DownloadResult = (Result.Result == EDownloadToMemoryResult::SucceededByPayload ? EDownloadToStorageResult::SucceededByPayload : EDownloadToStorageResult::Success), FileSavePath);

		if(bAutoDestroy)
		{
			DestroyDownload();
		}
	};

	if (bForceByPayload)
	{
		DownloadFileByPayload(URL, Timeout, ContentType).Next(OnResult);
	}
	else
	{
		DownloadFile(URL, Timeout, ContentType, (int64)TNumericLimits<TArray<uint8>::SizeType>::Max()).Next(OnResult);
	}
}

void FFileDownloader::CancelDownload()
{
	if(DownloadResult != EDownloadToStorageResult::None) return;
	
	OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::Cancelled, FileSavePath);
	if (HttpRequestPtr.IsValid())
	{
		const TSharedPtr<IHttpRequest> HttpRequest = HttpRequestPtr.Pin();

		HttpRequest->CancelRequest();
		
		HttpRequestPtr.Reset();
	}

	if(bAutoDestroy)
	{
		DestroyDownload();
	}
}

void FFileDownloader::DestroyDownload()
{
	if(DownloadResult == EDownloadToStorageResult::None) return;
	
	if (HttpRequestPtr.IsValid())
	{
		const TSharedPtr<IHttpRequest> HttpRequest = HttpRequestPtr.Pin();

		HttpRequest->CancelRequest();
	}

	OnDestroy_Internal();

	OnDestroy.Broadcast();
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
		WHLog(FString::Printf(TEXT("An error occurred while downloading the file to storage")), EDC_WebRequest, EDV_Error);
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::DownloadFailed, FileSavePath);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create save directory if it does not exist
	{
		if (!PlatformFile.DirectoryExists(*FilePath))
		{
			if (!PlatformFile.CreateDirectoryTree(*FilePath))
			{
				WHLog(FString::Printf(TEXT("Unable to create a directory '%s' to save the downloaded file"), *FilePath), EDC_WebRequest, EDV_Error);
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
			WHLog(FString::Printf(TEXT("Something went wrong while deleting the existing file '%s'"), *FileSavePath), EDC_WebRequest, EDV_Error);
			OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
			return;
		}
	}

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (!FileHandle)
	{
		WHLog(FString::Printf(TEXT("Something went wrong while saving the file '%s'"), *FileSavePath), EDC_WebRequest, EDV_Error);
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
		return;
	}

	if (!FileHandle->Write(DownloadedContent.GetData(), DownloadedContent.Num()))
	{
		WHLog(FString::Printf(TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath), EDC_WebRequest, EDV_Error);
		delete FileHandle;
		OnComplete.Broadcast(DownloadResult = EDownloadToStorageResult::SaveFailed, FileSavePath);
		return;
	}

	delete FileHandle;
}

void FFileDownloader::OnDestroy_Internal()
{
	WHLog(FString::Printf(TEXT("Download destroyed")), EDC_WebRequest, EDV_Warning);

	FWebRequestManager::Get().RemoveDownloader(SharedThis(this));
}

TFuture<FFileDownloaderResult> FFileDownloader::DownloadFile(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		WHLog(FString::Printf(TEXT("Canceled file download from %s"), *URL), EDC_WebRequest, EDV_Warning);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	TWeakPtr<FFileDownloader> WeakThisPtr = SharedThis(this);
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize](int64 ContentSize) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			WHLog(FString::Printf(TEXT("Canceled file download from %s"), *URL), EDC_WebRequest, EDV_Warning);
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
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
					PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
					return;
				}

				if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
				{
					WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
					PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
					return;
				}

				PromisePtr->SetValue(FFileDownloaderResult{Result.Result, MoveTemp(Result.Data)});
			});
		};
		
		if (ContentSize <= 0)
		{
			WHLog(FString::Printf(TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL), EDC_WebRequest, EDV_Warning);
			DownloadByPayload();
			return;
		}

		if (MaxChunkSize <= 0)
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: MaxChunkSize is <= 0. Trying to download the file by payload"), *URL), EDC_WebRequest, EDV_Error);
			DownloadByPayload();
			return;
		}

		TSharedPtr<TArray64<uint8>> OverallDownloadedDataPtr = MakeShared<TArray64<uint8>>();
		{
			WHLog(FString::Printf(TEXT("Pre-allocating %lld bytes for file download from %s"), ContentSize, *URL), EDC_WebRequest, EDV_Warning);
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
				WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
				PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
			{
				WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
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
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: data offset is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, *ChunkOffsetPtr, OverallDownloadedDataPtr->Num()), EDC_WebRequest, EDV_Error);
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}

				if (CurrentlyDownloadedSize > OverallDownloadedDataPtr->Num())
				{
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: overall downloaded size is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, CurrentlyDownloadedSize, OverallDownloadedDataPtr->Num()), EDC_WebRequest, EDV_Error);
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
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: download failed. Trying to download the file by payload"), *URL), EDC_WebRequest, EDV_Error);
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
		WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
		return MakeFulfilledPromise<EDownloadToMemoryResult>(EDownloadToMemoryResult::Cancelled).GetFuture();
	}

	TSharedPtr<TPromise<EDownloadToMemoryResult>> PromisePtr = MakeShared<TPromise<EDownloadToMemoryResult>>();
	TWeakPtr<FFileDownloader> WeakThisPtr = SharedThis(this);
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize, OnChunkDownloaded, ChunkRange](int64 ContentSize) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}
		
		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
			return;
		}

		if (ContentSize <= 0)
		{
			WHLog(FString::Printf(TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL), EDC_WebRequest, EDV_Warning);
			SharedThis->DownloadFileByPayload(URL, Timeout, ContentType).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnChunkDownloaded](FFileDownloaderResult Result) mutable
			{
				TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
				if (!SharedThis.IsValid())
				{
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
					PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
					return;
				}

				if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
				{
					WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
					PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
					return;
				}

				if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
				{
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result)), EDC_WebRequest, EDV_Error);
					PromisePtr->SetValue(Result.Result);
					return;
				}

				if (Result.Data.Num() <= 0)
				{
					WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloaded content is empty"), *URL), EDC_WebRequest, EDV_Error);
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
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: max chunk size is <= 0"), *URL), EDC_WebRequest, EDV_Error);
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
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: chunk range is out of range (%lld, expected [0, %lld])"), *URL, ChunkRange.Y, ContentSize), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}

		auto OnProgressInternal = [WeakThisPtr, URL, ChunkRange](int64 BytesSent, int64 BytesReceived, int64 ContentSize) mutable
		{
			TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
			if (SharedThis.IsValid())
			{
				const float Progress = ContentSize <= 0 ? 0.0f : static_cast<float>(BytesReceived + ChunkRange.X) / ContentSize;
				WHLog(FString::Printf(TEXT("Downloaded %lld bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *URL, ChunkRange.X, ChunkRange.Y, ContentSize, Progress), EDC_WebRequest, EDV_Log);
				SharedThis->OnProgress_Internal(BytesSent, BytesReceived + ChunkRange.X, ContentSize);
				SharedThis->OnProgress.Broadcast(URL, BytesSent, BytesReceived + ChunkRange.X, ContentSize);
			}
		};

		SharedThis->DownloadFileByChunk(URL, Timeout, ContentType, ContentSize, ChunkRange, OnProgressInternal).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, ContentSize, MaxChunkSize, OnChunkDownloaded, ChunkRange](FFileDownloaderResult&& Result)
		{
			TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
			if (!SharedThis.IsValid())
			{
				WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
				PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
				return;
			}

			if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
			{
				WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
				PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
				return;
			}

			if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
			{
				WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result)), EDC_WebRequest, EDV_Error);
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
		WHLog(FString::Printf(TEXT("Canceled file download from %s"), *URL), EDC_WebRequest, EDV_Warning);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.X < 0 || ChunkRange.Y <= 0 || ChunkRange.X > ChunkRange.Y)
	{
		WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is invalid"), *URL, ChunkRange.X, ChunkRange.Y), EDC_WebRequest, EDV_Error);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.Y - ChunkRange.X + 1 > ContentSize)
	{
		WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is out of range (%lld)"), *URL, ChunkRange.X, ChunkRange.Y, ContentSize), EDC_WebRequest, EDV_Error);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FFileDownloader> WeakThisPtr = SharedThis(this);

	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

	HttpRequestRef->SetTimeout(Timeout);

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
			WHLog(FString::Printf(TEXT("Downloaded %d bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentSize, Progress), EDC_WebRequest, EDV_Log);
			ProgressFun(BytesSent, BytesReceived, ContentSize);
		}
	});

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL, ChunkRange](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			WHLog(FString::Printf(TEXT("Canceled file chunk download from %s"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: request failed"), *Request->GetURL()), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: content length is 0"), *Request->GetURL()), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));

		if (ContentLength != ChunkRange.Y - ChunkRange.X + 1)
		{
			WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: content length (%lld) does not match the expected length (%lld)"), *Request->GetURL(), ContentLength, ChunkRange.Y - ChunkRange.X + 1), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		WHLog(FString::Printf(TEXT("Successfully downloaded file chunk from %s. Range: {%lld; %lld}, Overall: %lld"), *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentLength), EDC_WebRequest, EDV_Log);
		PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Success, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		WHLog(FString::Printf(TEXT("Failed to download file chunk from %s: request failed"), *URL), EDC_WebRequest, EDV_Error);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<FFileDownloaderResult> FFileDownloader::DownloadFileByPayload(const FString& URL, float Timeout, const FString& ContentType)
{
	if (DownloadResult == EDownloadToStorageResult::Cancelled)
	{
		WHLog(FString::Printf(TEXT("Canceled file download from %s"), *URL), EDC_WebRequest, EDV_Warning);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FFileDownloader> WeakThisPtr = SharedThis(this);

	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

	HttpRequestRef->SetTimeout(Timeout);

	HttpRequestRef->OnRequestProgress().BindLambda([WeakThisPtr, URL](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const int64 ContentLength = Request->GetContentLength();
			const float Progress = ContentLength <= 0 ? 0.0f : static_cast<float>(BytesReceived) / ContentLength;
			WHLog(FString::Printf(TEXT("Downloaded %d bytes of file chunk from %s by payload. Overall: %lld, Progress: %f"), BytesReceived, *Request->GetURL(), static_cast<int64>(Request->GetContentLength()), Progress), EDC_WebRequest, EDV_Log);
			SharedThis->OnProgress.Broadcast(URL, BytesSent, BytesReceived, ContentLength);
		}
	});

	TSharedPtr<TPromise<FFileDownloaderResult>> PromisePtr = MakeShared<TPromise<FFileDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FFileDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file from %s by payload: downloader has been destroyed"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->DownloadResult == EDownloadToStorageResult::Cancelled)
		{
			WHLog(FString::Printf(TEXT("Canceled file download from %s by payload"), *URL), EDC_WebRequest, EDV_Warning);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to download file from %s by payload: request failed"), *Request->GetURL()), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			WHLog(FString::Printf(TEXT("Failed to download file from %s by payload: content length is 0"), *Request->GetURL()), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		WHLog(FString::Printf(TEXT("Successfully downloaded file from %s by payload. Overall: %lld"), *Request->GetURL(), static_cast<int64>(Response->GetContentLength())), EDC_WebRequest, EDV_Log);
		return PromisePtr->SetValue(FFileDownloaderResult{EDownloadToMemoryResult::SucceededByPayload, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		WHLog(FString::Printf(TEXT("Failed to download file from %s by payload: request failed"), *URL), EDC_WebRequest, EDV_Error);
		return MakeFulfilledPromise<FFileDownloaderResult>(FFileDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<int64> FFileDownloader::GetContentSize(const FString& URL, float Timeout)
{
	TSharedPtr<TPromise<int64>> PromisePtr = MakeShared<TPromise<int64>>();

	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();

	HttpRequestRef->SetVerb("HEAD");
	HttpRequestRef->SetURL(URL);

	HttpRequestRef->SetTimeout(Timeout);

	HttpRequestRef->OnProcessRequestComplete().BindLambda([PromisePtr, URL](const FHttpRequestPtr& Request, const FHttpResponsePtr& Response, const bool bSucceeded)
	{
		if (!bSucceeded || !Response.IsValid())
		{
			WHLog(FString::Printf(TEXT("Failed to get size of file from %s: request failed"), *URL), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(0);
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));
		if (ContentLength <= 0)
		{
			WHLog(FString::Printf(TEXT("Failed to get size of file from %s: content length is %lld, expected > 0"), *URL, ContentLength), EDC_WebRequest, EDV_Error);
			PromisePtr->SetValue(0);
			return;
		}

		WHLog(FString::Printf(TEXT("Got size of file from %s: %lld"), *URL, ContentLength), EDC_WebRequest, EDV_Log);
		PromisePtr->SetValue(ContentLength);
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		WHLog(FString::Printf(TEXT("Failed to get size of file from %s: request failed"), *URL), EDC_WebRequest, EDV_Error);
		return MakeFulfilledPromise<int64>(0).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

void FFileDownloader::OnProgress_Internal(int64 BytesSent, int64 BytesReceived, int64 FullSize)
{
	IFileDownloaderInterface::OnProgress_Internal(BytesSent, BytesReceived, FullSize);
}
