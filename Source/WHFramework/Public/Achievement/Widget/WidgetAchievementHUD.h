// Copyright 2021 Sam Carey. All Rights Reserved..

#pragma once

#include "CoreMinimal.h"

#include "Achievement/AchievementModuleTypes.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "WidgetAchievementHUD.generated.h"

USTRUCT()
struct FAchievementPackage
{
	GENERATED_BODY()
	FAchievementData Data;
	FAchievementStates State;
};

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAchievementHUD : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAchievementHUD(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnCreate_Implementation(UObject* InOwner) override;

	virtual void OnInitialize_Implementation(UObject* InOwner) override;

	virtual void OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose_Implementation(bool bInstant) override;

	virtual void OnRefresh_Implementation() override;

	virtual void OnDestroy_Implementation(bool bRecovery = false) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Achievement HUD")
	void ScheduleAchievementDisplay(FAchievementData Achievement, FAchievementStates State);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCanvasPanel* Canvas;

	float CurrentTime;
	bool MovingUp;

	//Default spawning location for UIAchievement
	FVector2D SpawnLocation = FVector2D(0);
	FAnchors SpawnAnchors = FAnchors(0);
	FVector2D Size = FVector2D(0);

	//List to store active achievement + backlogged data 
	UPROPERTY()
	TArray<UWidgetAchievement*> Active;
	UPROPERTY()
	TArray<FAchievementPackage> BackLog;

	UFUNCTION()
	void OnAchievementDeath(UWidgetAchievement* Achievement);

	void Spawn(FAchievementData Achievement, FAchievementStates State);

	UPROPERTY()
	USoundBase* UnlockSound_Loaded;
	UPROPERTY()
	USoundBase* ProgressMadeSound_Loaded;
};
