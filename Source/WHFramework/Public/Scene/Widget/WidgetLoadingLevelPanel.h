// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "WidgetLoadingLevelPanel.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetLoadingLevelPanel : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetLoadingLevelPanel(const FObjectInitializer& objectInitializer);

public:
	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

	virtual void OnRefresh_Implementation() override;

protected:
	UPROPERTY(BlueprintReadOnly)
	FName LoadingLevelPath;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* ProgressBar_Progress;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_Progress;

private:
	float LoadProgress;

	float CurrentProgress;
	
public:
	UFUNCTION(BlueprintPure)
	float GetLoadProgress(bool bSmoothness = false) const { return bSmoothness ? CurrentProgress : LoadProgress; }

	UFUNCTION(BlueprintCallable)
	void SetLoadProgress(float InLoadProgress) { this->LoadProgress = InLoadProgress; }
};
