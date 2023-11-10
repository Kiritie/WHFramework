// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModule.h"

#include "SocketSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Network/NetworkModuleNetworkComponent.h"

IMPLEMENTATION_MODULE(UNetworkModule)

// ParamSets default values
UNetworkModule::UNetworkModule()
{
	ModuleName = FName("NetworkModule");
	ModuleDisplayName = FText::FromString(TEXT("Network Module"));

	ModuleNetworkComponent = UNetworkModuleNetworkComponent::StaticClass();

	bLocalMode = false;
	ServerURL = TEXT("");
	ServerPort = 7777;
}

UNetworkModule::~UNetworkModule()
{
	TERMINATION_MODULE(UNetworkModule)
}

#if WITH_EDITOR
void UNetworkModule::OnGenerate()
{
	Super::OnGenerate();
}

void UNetworkModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UNetworkModule)
}
#endif

void UNetworkModule::OnInitialize()
{
	Super::OnInitialize();
}

void UNetworkModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UNetworkModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UNetworkModule::OnPause()
{
	Super::OnPause();
}

void UNetworkModule::OnUnPause()
{
	Super::OnUnPause();
}

void UNetworkModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

FString UNetworkModule::GetServerURL() const
{
	return FString::Printf(TEXT("%s:%d"), *(!bLocalMode ? ServerURL : TEXT("127.0.0.1")), ServerPort);
}

FString UNetworkModule::GetServerLocalURL() const
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

FString UNetworkModule::GetServerLocalIP() const
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

int32 UNetworkModule::GetServerLocalPort() const
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

bool UNetworkModule::ConnectServer(const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	if(!InServerURL.IsEmpty()) ServerURL = InServerURL;
	
	if(InServerPort > 0) ServerPort = InServerPort;
	
	if(ServerURL.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(this, *GetServerURL(), true, InOptions);

	return true;
}

bool UNetworkModule::DisconnectServer(const FString& InLevelName)
{
	if(InLevelName.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(this, *InLevelName);

	return true;
}
