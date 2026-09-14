#include "Scene/Camera/CameraStreamingBridge.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
ACameraStreamingBridge::ACameraStreamingBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
}
void ACameraStreamingBridge::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FCameraViewSnapshot View = UCameraModuleStatics::GetViewSnapshot(PlayerIndex);
	SetActorTransform(View.Transform);
}
