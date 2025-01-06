// Copyright 2021 Sam Carey. All Rights Reserved..

#pragma once

#include "CoreMinimal.h"

#include "Achievement/AchievementModuleTypes.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Widget/Screen/UserWidgetBase.h"
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
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Achievement HUD")
	void ScheduleAchievementDisplay(FAchievementData Achievement, FAchievementStates State);

private:
	UPROPERTY(EditDefaultsOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
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
