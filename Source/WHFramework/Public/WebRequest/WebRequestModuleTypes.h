#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Parameter/ParameterModuleTypes.h"

#include "WebRequestModuleTypes.generated.h"

class UWebRequestHandleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, UWebRequestHandleBase*, InWebRequestHandle);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, UWebRequestHandleBase*, InWebRequestHandle);

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
		Content = TEXT("");
		ContentMap = TMap<FString, FString>();
		ContentType = EWebContentType::Form;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Content;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FParameterMap ContentMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWebContentType ContentType;

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
	
	FWebRequestResult(bool bInSucceeded, FHttpRequestPtr InHttpRequest, FHttpResponsePtr InHttpResponse)
	{
		bSucceeded = bInSucceeded;
		HttpRequest = InHttpRequest;
		HttpResponse = InHttpResponse;
	}
	
	UPROPERTY(BlueprintReadOnly)
	bool bSucceeded;

	FHttpRequestPtr HttpRequest;

	FHttpResponsePtr HttpResponse;
};