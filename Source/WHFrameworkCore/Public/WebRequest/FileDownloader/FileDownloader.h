// Georgy Treshchev 2021.

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "CoreMinimal.h"
#include "Http.h"
#include "Templates/SharedPointer.h"
#include "Async/Future.h"
#include "Misc/EngineVersionComparison.h"

UENUM()
enum class EDownloadFileType : uint8
{
	Resource,
	Custom1,
	Custom2,
	Custom3,
	Custom4,
	Custom5,
};

UENUM()
enum class EDownloadToMemoryResult : uint8
{
	Success,
	/** Downloaded successfully, but there was no Content-Length header in the response and thus downloaded by payload */
	SucceededByPayload,
	Cancelled,
	DownloadFailed,
	InvalidURL
};

UENUM()
enum class EDownloadToStorageResult : uint8
{
	None,
	Success,
	/** Downloaded successfully, but there was no Content-Length header in the response and thus downloaded by payload */
	SucceededByPayload,
	Cancelled,
	DownloadFailed,
	SaveFailed,
	DirectoryCreationFailed,
	InvalidURL,
	InvalidSavePath
};

const int32 BytesOfMB = 1024 * 1024;

/**
 * A struct that contains the result of downloading a file
 */
using FFileDownloaderResult = struct{ EDownloadToMemoryResult Result; TArray64<uint8> Data; };


DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFileDownloadProgress, int64/* BytesSent*/, int64/* BytesReceived*/, int64/* FullSize*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFileDownloadComplete, EDownloadToStorageResult/* Result*/, const FString&/* SavePath*/);

class WHFRAMEWORKCORE_API FFileDownloader : public TSharedFromThis<FFileDownloader>
{
public:
	FFileDownloader();
	virtual ~FFileDownloader();
	
public:
	/**
	 * Create a downloader
	 */
	static TSharedPtr<FFileDownloader> Create();
	
	/**
	 * Download the file and save it to the device disk
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param bForceByPayload If true, the file will be downloaded by payload even if the Content-Length header is present in the response
	 */
	void DownloadFile(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload);

	/**
	 * Download a file from the specified URL
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	TFuture<FFileDownloaderResult> DownloadFile(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize);

	/**
	 * Download a file by dividing it into chunks and downloading each chunk separately
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 * @param ChunkRange The range of chunks to download
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @param OnChunkDownloaded A function that is called when each chunk is downloaded
	 * @return A future that resolves to true if all chunks are downloaded successfully, false otherwise
	 */
	TFuture<EDownloadToMemoryResult> DownloadFilePerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, FInt64Vector2 ChunkRange, const TFunction<void(TArray64<uint8>&&)>& OnChunkDownloaded);

	/**
	 * Download a single chunk of a file
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param ContentSize The size of the file in bytes
	 * @param ChunkRange The range of the chunk to download
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	TFuture<FFileDownloaderResult> DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, FInt64Vector2 ChunkRange, const TFunction<void(int64, int64, int64)>& OnProgress);

	/**
	 * Download a file using payload-based approach. This approach is used when the server does not return the Content-Length header
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 * @note This approach cannot be used to download files that are larger than 2 GB
	 */
	TFuture<FFileDownloaderResult> DownloadFileByPayload(const FString& URL, float Timeout, const FString& ContentType);
	
	/**
	 * Get the content size of the file to be downloaded
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The timeout value in seconds
	 * @return A future that resolves to the content length of the file to be downloaded in bytes
	 */
	TFuture<int64> GetContentSize(const FString& URL, float Timeout);

	/**
	 * Cancel the download
	 */
	void CancelDownload();

	/**
	 * Destroy the download
	 */
	void DestroyDownload();

protected:
	virtual void OnComplete_Internal(EDownloadToMemoryResult Result, TArray64<uint8> DownloadedContent);

public:
	TWeakPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequestPtr;

	FOnFileDownloadProgress OnProgress;
	
	FOnFileDownloadComplete OnComplete;

	EDownloadFileType FileType;

	FString FileURL;
	FString FileSavePath;

	FString FilePath;
	FString FileName;
	FString FileExtension;

	EDownloadToStorageResult DownloadResult;
};