// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WebRequestModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WebRequestModuleBPLibrary.generated.h"

class UWebInterfaceBase;
class AWebRequestModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static bool IsWebLocalMode();

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void SetWebLocalMode(bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static FString GetWebServerURL();
	
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void SetWebServerURL(const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static int32 GetWebServerPort();

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void SetWebServerPort(int32 InServerPort);

public:
	template<class T>
	static bool HasWebInterface()
	{
		return HasWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static bool HasWebInterface(TSubclassOf<UWebInterfaceBase> InClass);

	template<class T>
	static T* GetWebInterface()
	{
		return Cast<T>(GetWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static UWebInterfaceBase* GetWebInterface(TSubclassOf<UWebInterfaceBase> InClass);

	template<class T>
	static T* CreateWebInterface()
	{
		return Cast<T>(CreateWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static UWebInterfaceBase* CreateWebInterface(TSubclassOf<UWebInterfaceBase> InClass);

	template<class T>
	static bool RegisterWebInterface(const FOnWebRequestComplete& InOnRequestComplete)
	{
		return RegisterWebInterface(T::StaticClass(), InOnRequestComplete);
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete);

	template<class T>
	static bool UnRegisterWebInterface(const FOnWebRequestComplete& InOnRequestComplete)
	{
		return UnRegisterWebInterface(T::StaticClass(), InOnRequestComplete);
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InClass, const FOnWebRequestComplete& InOnRequestComplete);

	template<class T>
	static bool UnRegisterAllWebInterface()
	{
		return UnRegisterAllWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InClass);

	template<class T>
	static bool ClearWebInterface()
	{
		return ClearWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool ClearWebInterface(TSubclassOf<UWebInterfaceBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void ClearAllWebInterface();

public:
	template<class T>
	static bool SendWebRequest(EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent())
	{
		return SendWebRequest(T::StaticClass(), InMethod, InParams, InHeadMap, InContent);
	}

	template<class T>
	static bool SendWebRequest(EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent())
	{
		return SendWebRequest(T::StaticClass(), InMethod, InParams, InHeadMap, InContent);
	}

	static bool SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>* InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	static bool SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send Web Request"), Category = "WebRequestModuleBPLibrary")
	static bool K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InClass, EWebRequestMethod InMethod, const TArray<FParameter>& InParams, FParameterMap InHeadMap, FWebContent InContent);
};
