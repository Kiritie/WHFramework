// Georgy Treshchev 2021.

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "CoreMinimal.h"
#include "FileDownloaderInterface.h"
#include "Templates/SharedPointer.h"
#include "Async/Future.h"

class WHFRAMEWORKCORE_API FFileDownloader : public IFileDownloaderInterface
{
public:
	FFileDownloader(FUniqueType InType, const FWebFileURL& InURL);
	FFileDownloader(const FWebFileURL& InURL);
	virtual ~FFileDownloader() override;
	
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
	virtual void DownloadFile(const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload);
	
	virtual void CancelDownload() override;

	virtual void DestroyDownload() override;

protected:
	/**
	 * Download a file from the specified URL
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param MaxChunkSize The maximum size of each InChunk to download in bytes
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	virtual TFuture<FFileDownloaderResult> DownloadFile(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize);

	/**
	 * Download a file by dividing it into chunks and downloading each InChunk separately
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param MaxChunkSize The maximum size of each InChunk to download in bytes
	 * @param ChunkRange The range of chunks to download
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @param OnChunkDownloaded A function that is called when each InChunk is downloaded
	 * @return A future that resolves to true if all chunks are downloaded successfully, false otherwise
	 */
	virtual TFuture<EDownloadToMemoryResult> DownloadFilePerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, FInt64Vector2 ChunkRange, const TFunction<void(TArray64<uint8>&&)>& OnChunkDownloaded);

	/**
	 * Download a single InChunk of a file
	 *
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param ContentSize The size of the file in bytes
	 * @param ChunkRange The range of the InChunk to download
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	virtual TFuture<FFileDownloaderResult> DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, FInt64Vector2 ChunkRange, const TFunction<void(int64, int64, int64)>& OnProgress);

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
	virtual TFuture<FFileDownloaderResult> DownloadFileByPayload(const FString& URL, float Timeout, const FString& ContentType);
	
	/**
	 * Get the content size of the file to be downloaded
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The timeout value in seconds
	 * @return A future that resolves to the content length of the file to be downloaded in bytes
	 */
	virtual TFuture<int64> GetContentSize(const FString& URL, float Timeout);

	virtual void DeleteFileHandle();

protected:
	virtual void OnProgress_Internal(FString CurrentFileUrl, int64 CurrentFileIndex, int64 TotalFileNum, int64 BytesSent, int64 BytesReceived, int64 FullSize) override;
	
	virtual void OnComplete_Internal(EDownloadToMemoryResult Result, TArray64<uint8> DownloadedContent);

	virtual void OnDestroy_Internal();

public:
	TWeakPtr<IHttpRequest> HttpRequestPtr;
	
	FWebFileURL FileURL;

	IFileHandle* FileHandle;

public:
	virtual FWebFileURL GetFileURL() const { return FileURL; }
};
