// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModule.h"
#include "Common/LifecycleTypes.h"

#include "SocketSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gameplay/WHGameInstance.h"

// ParamSets default values
UNetworkModule::UNetworkModule()
{
	ModuleName = FName("NetworkModule");
	ModuleDisplayName = FText::FromString(TEXT("Network Module"));

	bLocalMode = false;
	ServerURL = TEXT("");
	ServerPort = 7777;
}

UNetworkModule::~UNetworkModule()
{
}

UNetworkModule* UNetworkModule::GetPtr(const UGameInstance* InGameInstance)
{
	const UWHGameInstance* Owner = Cast<UWHGameInstance>(InGameInstance);
	return Owner ? Owner->FindModule<UNetworkModule>() : nullptr;
}

UNetworkModule* UNetworkModule::GetPtr(const UWorld* InWorld)
{
	return InWorld ? GetPtr(InWorld->GetGameInstance()) : nullptr;
}

#if WITH_EDITOR
void UNetworkModule::OnGenerate()
{
	Super::OnGenerate();
}

void UNetworkModule::OnDestroy()
{
	Super::OnDestroy();

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

void UNetworkModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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

FString UNetworkModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	DebugMessage.Appendf(TEXT("ServerURL: %s\n"), !bLocalMode ? (!ServerURL.IsEmpty() ? *ServerURL : TEXT("None")) : TEXT("127.0.0.1"));
	DebugMessage.Appendf(TEXT("ServerPort: %d"), ServerPort);
	return DebugMessage;
}

FString UNetworkModule::GetServerURL() const
{
	if (!bLocalMode && ServerURL.IsEmpty()) return FString();
	return FString::Printf(TEXT("%s:%d"), !bLocalMode ? *ServerURL : TEXT("127.0.0.1"), ServerPort);
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

bool UNetworkModule::ConnectServer(const UObject* WorldContextObject, const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UWHGameInstance* Instance = GetGameInstanceOwner();
	if (!World || World->bIsTearingDown || !Instance || World->GetGameInstance() != Instance || Instance->GetWorld() != World) return false;
	if(!InServerURL.IsEmpty()) ServerURL = InServerURL;
	
	if(InServerPort > 0) ServerPort = InServerPort;
	
	if(ServerURL.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(World, *GetServerURL(), true, InOptions);

	return true;
}

bool UNetworkModule::DisconnectServer(const UObject* WorldContextObject, const FString& InLevelName)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UWHGameInstance* Instance = GetGameInstanceOwner();
	if (!World || World->bIsTearingDown || !Instance || World->GetGameInstance() != Instance || Instance->GetWorld() != World || InLevelName.IsEmpty()) return false;
	
	UGameplayStatics::OpenLevel(World, *InLevelName);

	return true;
}
