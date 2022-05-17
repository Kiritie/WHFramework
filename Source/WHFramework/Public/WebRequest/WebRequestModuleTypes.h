#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

#include "WebRequestModuleTypes.generated.h"

class UWebRequestHandleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, UWebRequestHandleBase*, InWebRequestHandle);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, UWebRequestHandleBase*, InWebRequestHandle);

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWebRequestInfo
{
	GENERATED_BODY()
	
public:
	FWebRequestInfo()
	{
		bSucceeded = false;
		HttpRequestPtr = nullptr;
		HttpResponsePtr = nullptr;
	}
	
	FWebRequestInfo(bool bInSucceeded, FHttpRequestPtr InHttpRequest, FHttpResponsePtr InHttpResponse)
	{
		bSucceeded = bInSucceeded;
		HttpRequestPtr = InHttpRequest;
		HttpResponsePtr = InHttpResponse;
	}
	
	UPROPERTY(BlueprintReadOnly)
	bool bSucceeded;

	FHttpRequestPtr HttpRequestPtr;

	FHttpResponsePtr HttpResponsePtr;
};