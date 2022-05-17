// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WebRequestModuleTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Main/Base/ModuleBase.h"

#include "WebRequestModule.generated.h"

class UWebInterfaceBase;
UCLASS()
class WHFRAMEWORK_API AWebRequestModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AWebRequestModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

protected:
	UPROPERTY()
	TMap<TSubclassOf<UWebInterfaceBase>, UWebInterfaceBase*> WebInterfaces;

public:
	UFUNCTION(BlueprintCallable)
	bool HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	UWebInterfaceBase* GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	UWebInterfaceBase* CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	bool RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	UFUNCTION(BlueprintCallable)
	bool UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	UFUNCTION(BlueprintCallable)
	bool UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	bool ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable)
	void ClearAllWebInterface();

public:
	UFUNCTION(BlueprintCallable)
	bool SendWebRequestGet(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap);

	UFUNCTION(BlueprintCallable)
	bool SendWebRequestPost(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap, const FParameterMap& InParamMap, EParameterMapType InParamMapType = EParameterMapType::Text);

protected:
	bool SendWebRequestImpl(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, FParameterMap& InHeadMap, const FString& InContent = TEXT(""), bool bPost = false);

	void OnWebRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, UWebInterfaceBase* InWebInterface);
};
