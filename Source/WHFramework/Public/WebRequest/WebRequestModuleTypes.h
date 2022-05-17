#pragma once

#include "CoreMinimal.h"
#include "Handle/Base/WebRequestHandleBase.h"
#include "Interfaces/IHttpRequest.h"

#include "WebRequestModuleTypes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, UWebRequestHandleBase*, InWebRequestHandle)

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, UWebRequestHandleBase*, InWebRequestHandle)

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

	UPROPERTY(BlueprintReadOnly)
	FHttpRequestPtr HttpRequestPtr;

	UPROPERTY(BlueprintReadOnly)
	FHttpResponsePtr HttpResponsePtr;
};