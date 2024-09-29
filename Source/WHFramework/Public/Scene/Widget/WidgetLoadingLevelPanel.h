// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UserWidgetBase.h"
#include "WidgetLoadingLevelPanel.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetLoadingLevelPanel : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetLoadingLevelPanel(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

	virtual void OnRefresh() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	class UProgressBar* PBar_Progress;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	class UTextBlock* Txt_Progress;

	UPROPERTY(BlueprintReadOnly)
	FName LevelPath;

	UPROPERTY(BlueprintReadOnly)
	bool bUnloading;

private:
	float LoadProgress;

	float CurrentProgress;
	
public:
	UFUNCTION(BlueprintPure)
	float GetLoadProgress(bool bSmoothness = false) const;

	UFUNCTION(BlueprintCallable)
	void SetLoadProgress(float InLoadProgress, bool bSmoothness = false);
};
