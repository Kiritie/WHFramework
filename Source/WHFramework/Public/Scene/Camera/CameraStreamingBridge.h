#pragma once
#include "GameFramework/Info.h"
#include "CameraStreamingBridge.generated.h"
class UWorldPartitionStreamingSourceComponent;
class APlayerController;
UCLASS(NotPlaceable)
class WHFRAMEWORK_API ACameraStreamingBridge : public AInfo
{
	GENERATED_BODY()
public:
	ACameraStreamingBridge();
	virtual void Tick(float DeltaTime) override;
	void SetPlayer(APlayerController* InPlayer) { Player = InPlayer; }
private:
	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
	TWeakObjectPtr<APlayerController> Player;
};
