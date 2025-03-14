// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestModuleTypes.h"
#include "WebRequest/WebRequestManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WebRequestModuleStatics.generated.h"

class UWebInterfaceBase;
class UWebRequestModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static bool IsWebLocalMode();

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static void SetWebLocalMode(bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static FString GetWebServerURL();
	
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static void SetWebServerURL(const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static int32 GetWebServerPort();

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static void SetWebServerPort(int32 InServerPort);

public:
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static bool HasWebInterface(const FName InName);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static UWebInterfaceBase* GetWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass = nullptr);

	template<class T>
	static T* CreateWebInterface(const FName InName)
	{
		return Cast<T>(CreateWebInterface(InName, T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static UWebInterfaceBase* CreateWebInterface(const FName InName, TSubclassOf<UWebInterfaceBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool RegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool UnRegisterWebInterface(const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool UnRegisterAllWebInterface(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool ClearWebInterface(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static void ClearAllWebInterface();

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics")
	static FWebRequestConfig GetWebRequestConfig(const FName InName);

	static FWebRequestConfig CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	static FWebRequestConfig CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create Web Request Config"), Category = "WebRequestModuleStatics")
	FWebRequestConfig K2_CreateWebRequestConfig(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool ClearWebRequestConfig(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static void ClearAllWebRequestConfig();

public:
	static bool SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	static bool SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send Web Request", AutoCreateRefTerm = "InParams"), Category = "WebRequestModuleStatics")
	static bool K2_SendWebRequest(const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics")
	static bool SendWebRequestByConfig(const FName InName);
};
