// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFrameworkCoreTypes.h"
#include "Interfaces/IHttpRequest.h"

#include "WebRequestManager.generated.h"

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
struct WHFRAMEWORKCORE_API FWebContent
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
	FString ToString()
	{
		if(!Content.IsEmpty())
		{
			return Content;
		}
		else
		{
			if(ContentMap.GetNum() > 0)
			{
				FString ContentStr;
				switch(ContentType)
				{
					case EWebContentType::Form:
					{
						for(auto& Iter : ContentMap.GetMap())
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
		}
		return TEXT("");
	}
};

DECLARE_DELEGATE_OneParam(FOnWebRequestComplete_WithText, const FString&/*, Text*/);

DECLARE_DELEGATE_OneParam(FOnWebRequestComplete_WithJson, const TSharedPtr<FJsonObject>&/*, Json*/);

class WHFRAMEWORKCORE_API FWebRequestManager
{
public:	
	// ParamSets default values for this actor's properties
	FWebRequestManager();

	~FWebRequestManager();

public:
	static FWebRequestManager& Get();

public:
	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithText OnComplete);

	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithJson OnComplete);

protected:
	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, TFunction<void(FHttpResponsePtr, bool)> OnComplete);

	void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<void(FHttpResponsePtr, bool)> OnComplete);

protected:
	static FWebRequestManager* Instance;
	
	bool bLocalMode;

	FString ServerURL;

	int32 ServerPort;
	
public:
	bool IsLocalMode() const { return bLocalMode; }

	void SetLocalMode(bool bInLocalMode) { this->bLocalMode = bInLocalMode; }

	FString GetServerURL() const;
	
	void SetServerURL(const FString& InServerURL) { this->ServerURL = InServerURL; }

	int32 GetServerPort() const { return ServerPort; }

	void SetServerPort(int32 InServerPort) { this->ServerPort = InServerPort; }
};
