// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModule.h"

#include "SocketSubsystem.h"
#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Network/NetworkModuleNetworkComponent.h"

// ParamSets default values
ANetworkModule::ANetworkModule()
{
	ModuleName = FName("NetworkModule");

	bLocalMode = false;
	ServerURL = TEXT("");
	ServerPort = 7777;
}

#if WITH_EDITOR
void ANetworkModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ANetworkModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ANetworkModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ANetworkModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void ANetworkModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ANetworkModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ANetworkModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

FString ANetworkModule::GetServerURL() const
{
	return FString::Printf(TEXT("%s:%d"), *(!bLocalMode ? ServerURL : TEXT("127.0.0.1")), ServerPort);
}

FString ANetworkModule::GetServerLocalURL() const
{
	FString LocalIP = TEXT("");

	bool bCanBind = false;
	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);

	if(InternetAddr->IsValid())
	{
		LocalIP = InternetAddr->ToString(true);
	}

	return LocalIP;
}

FString ANetworkModule::GetServerLocalIP() const
{
	FString LocalIP = TEXT("");

	bool bCanBind = false;
	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);

	if(InternetAddr->IsValid())
	{
		LocalIP = InternetAddr->ToString(false);
	}

	return LocalIP;
}

int32 ANetworkModule::GetServerLocalPort() const
{
	int32 LocalPort = 0;

	bool bCanBind = false;
	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);

	if(InternetAddr->IsValid())
	{
		LocalPort = InternetAddr->GetPort();
	}

	return LocalPort;
}

bool ANetworkModule::ConnectServer(const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	if(!InServerURL.IsEmpty()) ServerURL = InServerURL;
	
	if(InServerPort > 0) ServerPort = InServerPort;
	
	if(ServerURL.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(this, *GetServerURL(), true, InOptions);

	return true;
}

bool ANetworkModule::DisconnectServer(const FString& InLevelName)
{
	if(InLevelName.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(this, *InLevelName);

	return true;
}
