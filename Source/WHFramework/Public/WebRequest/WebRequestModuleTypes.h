#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Parameter/ParameterModuleTypes.h"

#include "WebRequestModuleTypes.generated.h"

class UWebRequestHandleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, const UWebRequestHandleBase*, InWebRequestHandle);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, const UWebRequestHandleBase*, InWebRequestHandle);

UENUM(BlueprintType)
enum class EWebRequestMethod : uint8
{
	Get UMETA(DisplayName = "GET"),
	Post UMETA(DisplayName = "POST")
};

UENUM(BlueprintType)
enum class EWebContentType : uint8
{
	Form,
	Text,
	Json,
	Data
};

USTRUCT(BlueprintType)
struct FWebContent
{
	GENERATED_USTRUCT_BODY()

public:
	FWebContent()
	{
		ContentType = EWebContentType::Form;
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
	FString ToString();
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWebRequestResult
{
	GENERATED_BODY()
	
public:
	FWebRequestResult()
	{
		bSucceeded = false;
		HttpRequest = nullptr;
		HttpResponse = nullptr;
	}
	
	FWebRequestResult(FString InContent, bool bInSucceeded, FHttpRequestPtr InHttpRequest, FHttpResponsePtr InHttpResponse)
	{
		Content = InContent;
		bSucceeded = bInSucceeded;
		HttpRequest = InHttpRequest;
		HttpResponse = InHttpResponse;
	}
		
	UPROPERTY(BlueprintReadOnly)
	FString Content;

	UPROPERTY(BlueprintReadOnly)
	bool bSucceeded;

	FHttpRequestPtr HttpRequest;

	FHttpResponsePtr HttpResponse;
};