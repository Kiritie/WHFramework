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
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool IsWebLocalMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetWebLocalMode(const UObject* WorldContextObject, bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FString GetWebServerURL(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetWebServerURL(const UObject* WorldContextObject, const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static int32 GetWebServerPort(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetWebServerPort(const UObject* WorldContextObject, int32 InServerPort);

public:
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasWebInterface(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UWebInterfaceBase* GetWebInterface(const UObject* WorldContextObject, const FName InName, TSubclassOf<UWebInterfaceBase> InClass = nullptr);

	template<class T>
	static T* CreateWebInterface(const UObject* WorldContextObject, const FName InName)
	{
		return Cast<T>(CreateWebInterface(WorldContextObject, InName, T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UWebInterfaceBase* CreateWebInterface(const UObject* WorldContextObject, const FName InName, TSubclassOf<UWebInterfaceBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool RegisterWebInterface(const UObject* WorldContextObject, const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool UnRegisterWebInterface(const UObject* WorldContextObject, const FName InName, const FOnWebRequestComplete& InOnRequestComplete);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool UnRegisterAllWebInterface(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool ClearWebInterface(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllWebInterface(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FWebRequestConfig GetWebRequestConfig(const UObject* WorldContextObject, const FName InName);

	static FWebRequestConfig CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	static FWebRequestConfig CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent(), EWebRequestTriggerType InTriggerType = EWebRequestTriggerType::None, float InTriggerTime = 0.f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "Create Web Request Config"), Category = "WebRequestModuleStatics")
	static FWebRequestConfig K2_CreateWebRequestConfig(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent, EWebRequestTriggerType InTriggerType, float InTriggerTime);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool ClearWebRequestConfig(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllWebRequestConfig(const UObject* WorldContextObject);

public:
	static bool SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>* InParams = nullptr, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	static bool SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "Send Web Request", AutoCreateRefTerm = "InParams"), Category = "WebRequestModuleStatics")
	static bool K2_SendWebRequest(const UObject* WorldContextObject, const FName InName, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool SendWebRequestByConfig(const UObject* WorldContextObject, const FName InName);
};
