// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/UMG/UserWidgetBase.h"
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
	virtual void OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose_Implementation(bool bInstant) override;

	virtual void OnRefresh_Implementation() override;

protected:
	UPROPERTY(BlueprintReadOnly)
	FName LoadingLevelPath;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* PBarProgress;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TxtProgress;

private:
	float LoadProgress;
public:
	UFUNCTION(BlueprintPure)
	float GetLoadProgress() const { return LoadProgress; }

	UFUNCTION(BlueprintCallable)
	void SetLoadProgress(float InLoadProgress) { this->LoadProgress = InLoadProgress; }

private:
	float CurrentProgress;
};
