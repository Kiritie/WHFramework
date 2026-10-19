#pragma once

#include "Animation/WidgetAnimationHandle.h"
#include "Widget/Animator/WidgetAnimatorBase.h"
#include "WidgetAnimationAnimator.generated.h"

class UWidgetAnimation;
class UWidgetAnimationAnimator;

UCLASS()
class UWidgetAnimationCompletion : public UObject
{
	GENERATED_BODY()

	friend class UWidgetAnimationAnimator;

public:
	UFUNCTION()
	void Execute();

private:
	TWeakObjectPtr<UWidgetAnimationAnimator> Owner;
	uint64 Session = 0;
};

/** 播放控件已有的 UMG 动画，以实际完成事件驱动生命周期。 */
UCLASS(BlueprintType, EditInlineNew, HideCategories = (Animator))
class WHFRAMEWORK_API UWidgetAnimationAnimator : public UWidgetAnimatorBase
{
	GENERATED_BODY()

	friend class UWidgetAnimationCompletion;

public:
	UWidgetAnimationAnimator();

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;
	virtual void Play_Implementation(const FOnWidgetAnimatorCompleted& OnCompleted, bool bInstant) override;
	virtual bool Abort_Implementation() override;
	virtual void Pause_Implementation() override;
	virtual void Reset_Implementation() override;
	virtual void Stop_Implementation(bool bComplete) override;
	virtual void Complete_Implementation(bool bInstant) override;

	void Configure(FName InAnimationName, bool bInReverse = false);
	void SetReverse(bool bInReverse);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName AnimationName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bReverse = false;

private:
	void OnAnimationCompleted(uint64 InSession);

	void UnbindCompletion();
	void ApplyEndpoint(bool bEnd);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> Animation;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimationCompletion> CompletionBinding;

	FWidgetAnimationHandle Playback;
	TOptional<float> ResumePosition;
	FOnWidgetAnimatorCompleted Completion;
	uint64 PlaybackSession = 0;
	bool bPending = false;
};
