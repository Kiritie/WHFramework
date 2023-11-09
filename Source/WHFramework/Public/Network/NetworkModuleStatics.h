// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"

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
	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static bool IsNetworkLocalMode();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleStatics")
	static void SetNetworkLocalMode(bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static FString GetNetworkServerURL();
	
	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleStatics")
	static void SetNetworkServerURL(const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static int32 GetNetworkServerPort();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleStatics")
	static void SetNetworkServerPort(int32 InServerPort);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static FString GetServerLocalURL();

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static FString GetServerLocalIP();

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleStatics")
	static int32 GetServerLocalPort();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleStatics")
	static bool ConnectNetworkServer(const FString& InServerURL = TEXT(""), int32 InServerPort = -1, const FString& InOptions = FString(TEXT("")));

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleStatics")
	static bool DisconnectNetworkServer(const FString& InLevelName);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Actor Func
public:
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFunc(AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleStatics")
	static void ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
