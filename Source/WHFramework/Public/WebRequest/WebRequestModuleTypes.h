#pragma once

#include "Interfaces/IHttpRequest.h"

#include "WebRequestModuleTypes.generated.h"

class UWebRequestHandleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, const UWebRequestHandleBase*, InWebRequestHandle);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, const UWebRequestHandleBase*, InWebRequestHandle);

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