#pragma once
#include "GameFramework/Info.h"
#include "CameraStreamingBridge.generated.h"
class UWorldPartitionStreamingSourceComponent;
UCLASS(NotPlaceable)
class WHFRAMEWORK_API ACameraStreamingBridge : public AInfo
{
	GENERATED_BODY()
public:
	ACameraStreamingBridge();
	virtual void Tick(float DeltaTime) override;
	void SetPlayerIndex(int32 InPlayerIndex) { PlayerIndex = InPlayerIndex; }
private:
	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
	int32 PlayerIndex = 0;
};
