// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FWebRequestManager : public FManagerBase
{
	GENERATED_MANAGER(FWebRequestManager)

public:	
	// ParamSets default values for this actor's properties
	FWebRequestManager();

	virtual ~FWebRequestManager() override;
	
	static const FUniqueType Type;

public:
	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete);

	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithText OnComplete);

	bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithJson OnComplete);

protected:
	void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete);

protected:
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
