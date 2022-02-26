// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/UMG/UserWidgetBase.h"
#include "WidgetLoadingPanel.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetLoadingPanel : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetLoadingPanel(const FObjectInitializer& objectInitializer);

public:
	virtual void OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose_Implementation(bool bInstant) override;

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

protected:
	virtual void NativeConstruct() override;

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
