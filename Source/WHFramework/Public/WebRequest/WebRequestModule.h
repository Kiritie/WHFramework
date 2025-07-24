// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestModuleTypes.h"
#include "WebRequest/WebRequestManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Main/Base/ModuleBase.h"

#include "WebRequestModule.generated.h"

class UWebInterfaceBase;
UCLASS()
class WHFRAMEWORK_API UWebRequestModule : public UModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UWebRequestModule)

public:	
	// ParamSets default values for this actor's properties
	UWebRequestModule();

	~UWebRequestModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

public:
	virtual FString GetModuleDebugMessage() override;

protected:
	UPROPERTY(EditAnywhere)
	bool bLocalMode;

	UPROPERTY(EditAnywhere)
	FString ServerIP;

	UPROPERTY(EditAnywhere)
	int32 ServerPort;
public:
	UFUNCTION(BlueprintPure)
	bool IsLocalMode() const { return bLocalMode; }

	UFUNCTION(BlueprintCallable)
	void SetLocalMode(bool bInLocalMode);

	UFUNCTION(BlueprintPure)
	FString GetServerIP() const { return ServerIP; }

	UFUNCTION(BlueprintCallable)
	void SetServerIP(const FString& InServerIP);

	UFUNCTION(BlueprintPure)
	int32 GetServerPort() const { return ServerPort; }

	UFUNCTION(BlueprintCallable)
	void SetServerPort(int32 InServerPort);

	UFUNCTION(BlueprintPure)
	FString GetServerURL() const;

	UFUNCTION(BlueprintCallable)
	void SetServerURL(const FString& InServerURL);

protected:
	UPROPERTY(EditAnywhere, Instanced)
	TArray<UWebInterfaceBase*> WebInterfaces;

	UPROPERTY(EditAnywhere)
	TArray<FWebRequestConfig> WebRequestConfigs;

protected:
	UPROPERTY(Transient)
	TMap<FName, UWebInterfaceBase*> WebInterfaceMap;

	UPROPERTY(Transient)
	TMap<FName, FWebRequestConfig> WebRequestConfigMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasWebInterface(const FName InName);

	template<class T>
	T* GetWebInterface(const FName InName)
	{
		return Cast<T>(GetWebInterface(InName, T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWebInterfaceBase* GetWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass = nullptr);

	template<class T>
	T* CreateWebInterface(const FName InName)
	{
		return Cast<T>(GetWebInterface(InName, T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	UWebInterfaceBase* CreateWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass);
	
	UWebInterfaceBase* CreateWebInterface(UWebInterfaceBase* InWebInterface);

	UFUNCTION(BlueprintCallable)
	bool RegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable)
	bool UnRegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable)
	bool UnRegisterAllWebInterface(const FName InName);

	UFUNCTION(BlueprintCallable)
	bool ClearWebInterface(const FName InName);

	UFUNCTION(BlueprintCallable)
	void ClearAllWebInterface();

	UFUNCTION(BlueprintPure)
	bool HasWebRequestConfig(const FName InName);

	UFUNCTION(BlueprintPure)
	FWebRequestConfig GetWebRequestConfig(const FName InName);

	FWebRequestConfig CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	FWebRequestConfig CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create Web Request Config"))
	FWebRequestConfig K2_CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime);

	UFUNCTION(BlueprintCallable)
	bool ClearWebRequestConfig(const FName InName);

	UFUNCTION(BlueprintCallable)
	void ClearAllWebRequestConfig();

public:
	bool SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	bool SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send Web Request"))
	bool K2_SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent);

	UFUNCTION(BlueprintCallable)
	bool SendWebRequestByConfig(const FName InName);

protected:
	void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent, const TArray<FParameter> InParams);
};
