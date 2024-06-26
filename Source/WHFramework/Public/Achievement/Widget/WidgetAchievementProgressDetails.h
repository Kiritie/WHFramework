// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "WidgetAchievementProgressDetails.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAchievementProgressDetails : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (BindWidget))
	UTextBlock* Title;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (BindWidget))
	UTextBlock* ProgressText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (BindWidget))
	UProgressBar* AchievementProgressBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details")
	FString TitleText = "Achievement Progress";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details")
	FLinearColor ProgressBarColour = FLinearColor(0,0.5, 1,1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details")
	int32 TotalProgressMaxDecimalPlaces = 2;

	UFUNCTION(BlueprintCallable, Category = "Achievement Progress Details")
	void UpdateDetails();
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
};