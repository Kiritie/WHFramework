#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Parameter/ParameterTypes.h"
#include "WebRequest/WebRequestTypes.h"

#include "WebRequestModuleTypes.generated.h"

class UWebRequestHandleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebRequestCompleteMulti, const UWebRequestHandleBase*, InWebRequestHandle);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWebRequestComplete, const UWebRequestHandleBase*, InWebRequestHandle);

UENUM(BlueprintType)
enum class EWebRequestTriggerType : uint8
{
	None,
	Default,
	Timer,
	Procedure
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWebRequestConfig
{
	GENERATED_BODY()
	
public:
	FWebRequestConfig()
	{
		Name = NAME_None;
		Method = EWebRequestMethod::Get;
		Params = TArray<FParameter>();
		HeadMap = FParameterMap();
		Content = FWebContent();
		TriggerType = EWebRequestTriggerType::None;
		TriggerTime = 0.f;
		TriggerTimerHandle = FTimerHandle();
	}
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWebRequestMethod Method;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FParameter> Params;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameterMap HeadMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWebContent Content;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWebRequestTriggerType TriggerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "TriggerType == EWebRequestTriggerType::Timer"))
	float TriggerTime;

	UPROPERTY(Transient)
	FTimerHandle TriggerTimerHandle;
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