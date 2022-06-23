// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	static FString GetServerURL();
	
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void SetServerURL(const FString& InServerURL);

public:
	template<class T>
	static bool HasWebInterface()
	{
		return HasWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static bool HasWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	static T* GetWebInterface()
	{
		return Cast<T>(GetWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, Category = "WebRequestModuleBPLibrary")
	static UWebInterfaceBase* GetWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	static T* CreateWebInterface()
	{
		return Cast<T>(CreateWebInterface(T::StaticClass()));
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static UWebInterfaceBase* CreateWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	static bool RegisterWebInterface(const FOnWebRequestComplete& InOnWebRequestComplete)
	{
		return RegisterWebInterface(T::StaticClass(), InOnWebRequestComplete);
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool RegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	template<class T>
	static bool UnRegisterWebInterface(const FOnWebRequestComplete& InOnWebRequestComplete)
	{
		return UnRegisterWebInterface(T::StaticClass(), InOnWebRequestComplete);
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool UnRegisterWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, const FOnWebRequestComplete& InOnWebRequestComplete);

	template<class T>
	static bool UnRegisterAllWebInterface()
	{
		return UnRegisterAllWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool UnRegisterAllWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	template<class T>
	static bool ClearWebInterface()
	{
		return ClearWebInterface(T::StaticClass());
	}
	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static bool ClearWebInterface(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass);

	UFUNCTION(BlueprintCallable, Category = "WebRequestModuleBPLibrary")
	static void ClearAllWebInterface();

public:
	template<class T>
	static bool SendWebRequest(EWebRequestMethod InMethod, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent())
	{
		return SendWebRequest(T::StaticClass(), InMethod, InHeadMap, InContent);
	}

	static bool SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap = FParameterMap(), FWebContent InContent = FWebContent());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send Web Request"), Category = "WebRequestModuleBPLibrary")
	static bool K2_SendWebRequest(TSubclassOf<UWebInterfaceBase> InWebInterfaceClass, EWebRequestMethod InMethod, FParameterMap InHeadMap, FWebContent InContent);
};
