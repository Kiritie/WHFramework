// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "NetworkModuleStatics.generated.h"

class UNetworkModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UNetworkModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static bool IsNetworkLocalMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static void SetNetworkLocalMode(const UObject* WorldContextObject, bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static FString GetNetworkServerURL(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintCallable, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static void SetNetworkServerURL(const UObject* WorldContextObject, const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static int32 GetNetworkServerPort(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static void SetNetworkServerPort(const UObject* WorldContextObject, int32 InServerPort);

	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static FString GetServerLocalURL(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static FString GetServerLocalIP(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static int32 GetServerLocalPort(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static bool ConnectNetworkServer(const UObject* WorldContextObject, const FString& InServerURL = TEXT(""), int32 InServerPort = -1, const FString& InOptions = FString(TEXT("")));

	UFUNCTION(BlueprintCallable, Category = "NetworkModule", meta = (WorldContext = "WorldContextObject"))
	static bool DisconnectNetworkServer(const UObject* WorldContextObject, const FString& InLevelName);

};
