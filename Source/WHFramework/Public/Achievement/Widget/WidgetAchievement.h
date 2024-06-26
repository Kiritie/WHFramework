// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "TimerManager.h"
#include "Achievement/AchievementModuleTypes.h"
#include "WidgetAchievement.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeathEvent, UWidgetAchievement *, Achievement);

/**
 * 
 */
UCLASS(HideDropdown)
class WHFRAMEWORK_API UWidgetAchievement : public UUserWidget
{
	GENERATED_BODY()

public:
	float StartHeight;
	FDeathEvent OnDeath;

	void StartDeathTimer(float ExitAnimationLength, float ScreenTime, EAchievementAnimation ExitAnimation, bool SlideAnimationRight);

	UFUNCTION(Category="UI Achievement")
	void SetValue(FAchievementData Achievement, FAchievementStates State, bool ShowExactProgress);

	UPROPERTY(meta = (BindWidget))
	USizeBox* Root;

private:
	//Components
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Title;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Description;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PercentText;
	UPROPERTY(meta = (BindWidget))
	UImage* Icon;
	UPROPERTY(meta = (BindWidget))
	UImage* OverlayIcon;
	UPROPERTY(meta = (BindWidget))
	UProgressBar* ProgressBar;

	//Animations
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	UWidgetAnimation* SlideLeft;
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	UWidgetAnimation* SlideRight;
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	UWidgetAnimation* ScaleDown;
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	UWidgetAnimation* Fade;

	FTimerHandle DeathTimer;
	FTimerHandle DeathTimerAnimationComplete;

	UFUNCTION(Category = "UI Achievement")
	void OnDeathEvent(UWidgetAnimation* Animation, float AnimationLength);

	UFUNCTION(Category = "UI Achievement")
	void OnDeathAnimationComplete();
};
