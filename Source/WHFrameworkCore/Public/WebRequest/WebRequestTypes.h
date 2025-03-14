// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterTypes.h"
#include "WebRequestTypes.generated.h"

const int32 BytesOfMB = 1024 * 1024;

/**
 * A struct that contains the result of downloading a file
 */
using FFileDownloaderResult = struct{ EDownloadToMemoryResult Result; TArray64<uint8> Data; };

DECLARE_MULTICAST_DELEGATE_SixParams(FOnFileDownloadProgress, FString/* CurrentFileUrl*/, int64/* CurrentFileIndex*/, int64/* TotalFileNum*/, int64/* BytesSent*/, int64/* BytesReceived*/, int64/* FullSize*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFileDownloadComplete, EDownloadToStorageResult/* Result*/, const FString&/* SavePath*/);

DECLARE_MULTICAST_DELEGATE(FOnFileDownloadDestroy)

DECLARE_DELEGATE_OneParam(FOnWebRequestComplete_WithText, const FString&/*, Text*/);

DECLARE_DELEGATE_OneParam(FOnWebRequestComplete_WithJson, const TSharedPtr<FJsonObject>&/*, JsonObject*/);

UENUM(BlueprintType)
enum class EWebRequestMethod : uint8
{
	Get UMETA(DisplayName = "GET"),
	Post UMETA(DisplayName = "POST")
};

UENUM(BlueprintType)
enum class EWebContentType : uint8
{
	Form_Url,
	Form_Body,
	Text,
	Json,
	Data
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

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FWebFileURL
{
	GENERATED_USTRUCT_BODY()

public:
	FWebFileURL()
	{
		FileURL = TEXT("");
		FilePath = TEXT("");
		MD5Hash = TEXT("");
	}

	FWebFileURL(const FString& InFileURL) : FWebFileURL()
	{
		FileURL = InFileURL;
	}

	FWebFileURL(const FString& InFileURL, const FString& InFilePath) : FWebFileURL()
	{
		FileURL = InFileURL;
		FilePath = InFilePath;
	}

	FWebFileURL(const FString& InFileURL, const FString& InFilePath, const FString& InMD5Hash) : FWebFileURL()
	{
		FileURL = InFileURL;
		FilePath = InFilePath;
		MD5Hash = InMD5Hash;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString FileURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString FilePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString MD5Hash;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FWebContent
{
	GENERATED_USTRUCT_BODY()

public:
	FWebContent()
	{
		ContentType = EWebContentType::Form_Body;
		Content = TEXT("");
		ContentMap = FParameterMap();
		ContentData = TArray<uint8>();
	}

	FWebContent(EWebContentType InContentType) : FWebContent()
	{
		ContentType = InContentType;
	}

	FWebContent(EWebContentType InContentType, const FString& InContent) : FWebContent()
	{
		ContentType = InContentType;
		Content = InContent;
	}

	FWebContent(EWebContentType InContentType, const FParameterMap& InContentMap) : FWebContent()
	{
		ContentType = InContentType;
		ContentMap = InContentMap;
	}

	FWebContent(EWebContentType InContentType, const TArray<uint8>& InContentData) : FWebContent()
	{
		ContentType = InContentType;
		ContentData = InContentData;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWebContentType ContentType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Content;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FParameterMap ContentMap;

	/**
	 * File Upload Example ------------------------
	 *
	 * // Request header
	 * FString Boundary = "---------------------------" + FString::FromInt(FDateTime::Now().GetTicks());
	 * 
	 * // Begin Boundary
	 * FString BeginBoundary = "\r\n--" + Boundary + "\r\n";
	 * 
	 * // Begin Boundary
	 * ContentData.Append((uint8*)TCHAR_TO_ANSI(*BeginBoundary), BeginBoundary.Len());
	 * 
	 * // Describe Header
	 * FString DescribeHeader = "Content-Disposition: form-data;name=\"describe\"";
	 * DescribeHeader.Append("\r\n\r\n");
	 * ContentData.Append((uint8*)TCHAR_TO_ANSI(*DescribeHeader), DescribeHeader.Len());
	 * FTCHARToUTF8 DescribeUTF8(*Describe, Describe.Len());
	 * ContentData.Append((UTF8CHAR*)DescribeUTF8.Get(), DescribeUTF8.Length() * sizeof(UTF8CHAR));
	 * 
	 * // The bit array format of a single file
	 * 
	 * TArray<uint8> FileContent;
	 * if (FFileHelper::LoadFileToArray(FileContent, *FilePath))
	 * {
	 * 	// Begin Boundary
	 * 	ContentData.Append((uint8*)TCHAR_TO_ANSI(*BeginBoundary), BeginBoundary.Len());
	 * 
	 * 	// File Header
	 * 	FString FileHeader = "Content-Disposition: form-data;name=\"imgs\";";
	 * 	FileHeader.Append("filename=\"" + FPaths::GetCleanFilename(*FilePath) + "\"\r\n");
	 * 	FileHeader.Append("Content-Type: application/octet-stream\r\n");
	 * 	FileHeader.Append("Content-Transfer-Encoding: binary\r\n\r\n");
	 * 	ContentData.Append((uint8*)TCHAR_TO_ANSI(*FileHeader), FileHeader.Len());
	 * 
	 * 	// File Content
	 * 	ContentData.Append(FileContent);
	 * }
	 * 
	 * // End Boundary
	 * FString EndBoundary = "\r\n--" + Boundary + "--\r\n";
	 * ContentData.Append((uint8*)TCHAR_TO_ANSI(*EndBoundary), EndBoundary.Len());
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<uint8> ContentData;

public:
	FString ToString() const
	{
		if(!Content.IsEmpty())
		{
			return Content;
		}
		else if(ContentMap.GetNum() > 0)
		{
			FString ContentStr;
			switch(ContentType)
			{
				case EWebContentType::Form_Url:
				case EWebContentType::Form_Body:
				{
					for(auto& Iter : ContentMap.GetSource())
					{
						ContentStr.Append(FString::Printf(TEXT("%s=%s&"), *Iter.Key, *Iter.Value));
					}
					ContentStr.RemoveFromEnd(TEXT("&"));
					break;
				}
				case EWebContentType::Text:
				{
					ContentStr = ContentMap.ToString();
					break;
				}
				case EWebContentType::Json:
				{
					ContentStr = ContentMap.ToJsonString();
					break;
				}
				default: break;
			}
			return ContentStr;
		}
		return TEXT("");
	}
};
