// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleStatics.h"

#include "Network/NetworkModule.h"
#include "Engine/World.h"

namespace
{
	UNetworkModule& ResolveNetworkModule(const UObject* InContext)
	{
		UNetworkModule* Module = InContext ? UNetworkModule::GetPtr(InContext->GetWorld()) : nullptr;
		checkf(Module, TEXT("Network module requires a GameInstance and world context"));
		return *Module;
	}
}

bool UNetworkModuleStatics::IsNetworkLocalMode(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).IsLocalMode();
}

void UNetworkModuleStatics::SetNetworkLocalMode(const UObject* WorldContextObject, bool bInLocalMode)
{
	ResolveNetworkModule(WorldContextObject).SetLocalMode(bInLocalMode);
}

FString UNetworkModuleStatics::GetNetworkServerURL(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).GetServerURL();
}

void UNetworkModuleStatics::SetNetworkServerURL(const UObject* WorldContextObject, const FString& InServerURL)
{
	ResolveNetworkModule(WorldContextObject).SetServerURL(InServerURL);
}

int32 UNetworkModuleStatics::GetNetworkServerPort(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).GetServerPort();
}

void UNetworkModuleStatics::SetNetworkServerPort(const UObject* WorldContextObject, int32 InServerPort)
{
	ResolveNetworkModule(WorldContextObject).SetServerPort(InServerPort);
}

FString UNetworkModuleStatics::GetServerLocalURL(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).GetServerLocalURL();
}

FString UNetworkModuleStatics::GetServerLocalIP(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).GetServerLocalIP();
}

int32 UNetworkModuleStatics::GetServerLocalPort(const UObject* WorldContextObject)
{
	return ResolveNetworkModule(WorldContextObject).GetServerLocalPort();
}

bool UNetworkModuleStatics::ConnectNetworkServer(const UObject* WorldContextObject, const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	return ResolveNetworkModule(WorldContextObject).ConnectServer(WorldContextObject, InServerURL, InServerPort, InOptions);
}

bool UNetworkModuleStatics::DisconnectNetworkServer(const UObject* WorldContextObject, const FString& InLevelName)
{
	return ResolveNetworkModule(WorldContextObject).DisconnectServer(WorldContextObject, InLevelName);
}
