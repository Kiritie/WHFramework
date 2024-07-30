// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class IFileDownloaderInterface;
class FFileDownloaderCollection;
class FFileDownloader;

class WHFRAMEWORKCORE_API FWebRequestManager : public FManagerBase
{
	GENERATED_MANAGER(FWebRequestManager)

public:	
	// ParamSets default values for this actor's properties
	FWebRequestManager();

	virtual ~FWebRequestManager() override;
	
	static const FUniqueType Type;

public:
	virtual void OnInitialize() override;
	
	virtual void OnPreparatory() override;

	virtual void OnRefresh(float DeltaSeconds) override;

public:
	virtual void ConnectServer(); 

public:
	virtual bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete);

	virtual bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithText OnComplete);

	virtual bool SendWebRequest(const FString& InUrl, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent, FOnWebRequestComplete_WithJson OnComplete);

	virtual bool CancelWebRequest(const FString& InUrl);

	virtual TSharedPtr<IHttpRequest> GetWebRequest(const FString& InUrl) const;

public:
	virtual void AddDownloader(const TSharedPtr<IFileDownloaderInterface>& Downloader);

	virtual void RemoveDownloader(const TSharedPtr<IFileDownloaderInterface>& Downloader);

protected:
	virtual void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> OnComplete);

protected:
	bool bLocalMode;

	FString ServerURL;

	int32 ServerPort;

	bool bConnected;

	TMap<FString, TSharedPtr<IHttpRequest>> WebRequestMap;

	TArray<TSharedPtr<IFileDownloaderInterface>> Downloaders;

public:
	bool IsLocalMode() const { return bLocalMode; }

	void SetLocalMode(bool bInLocalMode) { this->bLocalMode = bInLocalMode; }

	FString GetServerURL() const;
	
	void SetServerURL(const FString& InServerURL);

	int32 GetServerPort() const { return ServerPort; }

	void SetServerPort(int32 InServerPort) { ServerPort = InServerPort; }

	bool IsConnected() const { return bConnected; }

	TArray<TSharedPtr<IFileDownloaderInterface>> GetAllDownloader() { return Downloaders; }
	
	template<class T>
	TArray<TSharedPtr<T>> GetAllDownloader()
	{
		TArray<TSharedPtr<T>> ReturnValues;
		for(auto& Iter : TArray(Downloaders))
		{
			if(Iter && Iter->IsA<T>())
			{
				ReturnValues.Add(StaticCastSharedPtr<T>(Iter));
			}
		}
		return ReturnValues;
	}
};
