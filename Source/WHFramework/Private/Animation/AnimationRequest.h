#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AnimationRequest.generated.h"

class UAnimationModule;

UCLASS()
class UAnimationRequest : public UObject
{
	GENERATED_BODY()
public:
	void Initialize(UAnimationModule& InModule, UWorld& InWorld, UObject& InOwner, const FGuid& InId, TFunction<void()> InFinish, TFunction<void()> InCancel);
	void Cancel();
	UFUNCTION()
	void Complete(int32 Linkage);
private:
	void Dispatch();
	TWeakObjectPtr<UAnimationModule> Module;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UObject> Owner;
	FGuid Id;
	TFunction<void()> Finish;
	TFunction<void()> Canceled;
	bool bFinished = false;
	bool bDispatchPending = false;
};
