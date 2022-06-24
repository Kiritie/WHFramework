// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"

#include "NetworkModuleBPLibrary.generated.h"

class ANetworkModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UNetworkModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
public:
	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static bool IsNetworkLocalMode();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleBPLibrary")
	static void SetNetworkLocalMode(bool bInLocalMode);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static FString GetNetworkServerURL();
	
	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleBPLibrary")
	static void SetNetworkServerURL(const FString& InServerURL);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static int32 GetNetworkServerPort();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleBPLibrary")
	static void SetNetworkServerPort(int32 InServerPort);

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static FString GetServerLocalURL();

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static FString GetServerLocalIP();

	UFUNCTION(BlueprintPure, Category = "NetworkRequestModuleBPLibrary")
	static int32 GetServerLocalPort();

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleBPLibrary")
	static bool ConnectNetworkServer(const FString& InServerURL = TEXT(""), int32 InServerPort = -1, const FString& InOptions = FString(TEXT("")));

	UFUNCTION(BlueprintCallable, Category = "NetworkRequestModuleBPLibrary")
	static bool DisconnectNetworkServer(const FString& InLevelName);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Actor Func
public:
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFunc(AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
