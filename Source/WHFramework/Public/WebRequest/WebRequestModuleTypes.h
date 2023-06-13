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
	Json
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
		ContentMap = TMap<FString, FString>();
	}

	FWebContent(EWebContentType InContentType)
	{
		ContentType = InContentType;
		Content = TEXT("");
		ContentMap = TMap<FString, FString>();
	}

	FWebContent(EWebContentType InContentType, const FString& InContent)
	{
		ContentType = InContentType;
		Content = InContent;
		ContentMap = TMap<FString, FString>();
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWebContentType ContentType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Content;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FParameterMap ContentMap;

public:
	FORCEINLINE FParameterMap& operator()()
	{
		return ContentMap;
	}

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