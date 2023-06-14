// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestModuleTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Main/Base/ModuleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "WebRequestModule.generated.h"

class UWebInterfaceBase;
UCLASS()
class WHFRAMEWORK_API AWebRequestModule : public AModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(AWebRequestModule)

public:	
	// ParamSets default values for this actor's properties
	AWebRequestModule();

	~AWebRequestModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

protected:
	UPROPERTY(EditAnywhere)
	bool bLocalMode;

	UPROPERTY(EditAnywhere)
	FString ServerURL;

	UPROPERTY(EditAnywhere)
	int32 ServerPort;
public:
	UFUNCTION(BlueprintPure)
	bool IsLocalMode() const { return bLocalMode; }

	UFUNCTION(BlueprintCallable)
	void SetLocalMode(bool bInLocalMode) { this->bLocalMode = bInLocalMode; }

	UFUNCTION(BlueprintPure)
	FString GetServerURL() const;

	UFUNCTION(BlueprintCallable)
	void SetServerURL(const FString& InServerURL) { this->ServerURL = InServerURL; }

	UFUNCTION(BlueprintPure)
	int32 GetServerPort() const { return ServerPort; }

	UFUNCTION(BlueprintCallable)
	void SetServerPort(int32 InServerPort) { this->ServerPort = InServerPort; }

protected:
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UWebInterfaceBase>> WebInterfaces;

protected:
	UPROPERTY(VisibleAnywhere)
	TMap<TSubclassOf<UWebInterfaceBase>, UWebInterfaceBase*> WebInterfaceMap;

public:
	template<class T>
	bool HasWebInterface()
	{
		return HasWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable)
	bool HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	T* GetWebInterface()
	{
		return Cast<T>(GetWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable)
	UWebInterfaceBase* GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	T* CreateWebInterface()
	{
		return Cast<T>(GetWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable)
	UWebInterfaceBase* CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	bool RegisterWebInterface(const FOnWebRequestComplete& InOnWebRequestComplete)
	{
		return RegisterWebInterface(T::StaticClass(), InOnWebRequestComplete);
	}
	UFUNCTION(BlueprintCallable)
	bool RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	template<class T>
	bool UnRegisterWebInterface(const FOnWebRequestComplete& InOnWebRequestComplete)
	{
		return UnRegisterWebInterface(T::StaticClass(), InOnWebRequestComplete);
	}
	UFUNCTION(BlueprintCallable)
	bool UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	template<class T>
	bool UnRegisterAllWebInterface()
	{
		return UnRegisterAllWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable)
	bool UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	bool ClearWebInterface()
	{
		return ClearWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable)
	bool ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	void ClearAllWebInterface();

public:
	template<class T>
	bool SendWebRequest(EWebRequestMethod InMethod, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent())
	{
		return SendWebRequest(T::StaticClass(), InMethod, InHeadMap, InContent);
	}

	bool SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send Web Request"))
	bool K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent);

protected:
	void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface, const FString InContent);
};
