#include "Scene/Camera/CameraStreamingBridge.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Scene/SceneModule.h"
ACameraStreamingBridge::ACameraStreamingBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
	StreamingSource->DisableStreamingSource();
}
void ACameraStreamingBridge::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* Controller = Player.Get();
	if (!Controller || Controller->GetWorld() != GetWorld() || !Controller->IsLocalController() || !Controller->PlayerCameraManager)
	{
		StreamingSource->DisableStreamingSource();
		return;
	}
	FVector Location;
	FRotator Rotation;
	Controller->GetPlayerViewPoint(Location, Rotation);
	SetActorLocationAndRotation(Location, Rotation);
	StreamingSource->EnableStreamingSource();
}

void USceneModule::RefreshCameraStreamingBridges()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bRefreshingCameraStreamingBridges) return;
	TGuardValue<bool> RefreshGuard(bRefreshingCameraStreamingBridges, true);
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	if (bSceneRuntimeClosing || !World || World->bIsTearingDown || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer || !Instance)
	{
		ClearCameraStreamingBridges();
		return;
	}
	const uint32 Generation = CameraStreamingBridgeGeneration;
	const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
	TArray<ACameraStreamingBridge*> Removed;
	for (auto It = CameraStreamingBridges.CreateIterator(); It; ++It)
	{
		ULocalPlayer* LocalPlayer = It.Key();
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Players.Contains(LocalPlayer) || !Controller || !Controller->IsLocalController() || !::IsValid(It.Value()))
		{
			if (::IsValid(It.Value())) Removed.Add(It.Value());
			It.RemoveCurrent();
		}
	}
	for (ACameraStreamingBridge* Bridge : Removed) Bridge->Destroy();
	if (!IsBusinessAdmissionOpen() || Generation != CameraStreamingBridgeGeneration) return;
	for (ULocalPlayer* LocalPlayer : Players)
	{
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Controller || !Controller->IsLocalController() || Controller->GetWorld() != World) continue;
		ACameraStreamingBridge* Bridge = CameraStreamingBridges.FindRef(LocalPlayer);
		if (!::IsValid(Bridge))
		{
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.Owner = GetModuleOwner();
			Bridge = World->SpawnActor<ACameraStreamingBridge>(Spawn);
			if (!IsBusinessAdmissionOpen() || Generation != CameraStreamingBridgeGeneration || World->bIsTearingDown)
			{
				if (::IsValid(Bridge)) Bridge->Destroy();
				return;
			}
			if (!Bridge) continue;
			CameraStreamingBridges.Add(LocalPlayer, Bridge);
		}
		Bridge->SetPlayer(Controller);
	}
}

void USceneModule::ClearCameraStreamingBridges()
{
	++CameraStreamingBridgeGeneration;
	TArray<TObjectPtr<ACameraStreamingBridge>> Bridges;
	CameraStreamingBridges.GenerateValueArray(Bridges);
	CameraStreamingBridges.Reset();
	for (ACameraStreamingBridge* Bridge : Bridges) if (::IsValid(Bridge)) Bridge->Destroy();
}
