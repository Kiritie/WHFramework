// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Widget/WidgetAchievementProgressDetails.h"

#include "Achievement/AchievementModule.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet/KismetStringLibrary.h"

void UWidgetAchievementProgressDetails::UpdateDetails()
{
	Title->SetText(FText::FromString(TitleText));
	AchievementProgressBar->SetFillColorAndOpacity(ProgressBarColour);
	Title->SetVisibility(TitleText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	int32 TotalAchieved = AAchievementModule::Get()->GetAchievedAchievementCount();
	int32 Total = AAchievementModule::Get()->Achievements.Num();
	float Progress = (float)TotalAchieved / (float)Total;

	FString ProgressString = UKismetTextLibrary::Conv_DoubleToText(Progress * 100, ERoundingMode::HalfFromZero, false, false, 1, 324, 0, TotalProgressMaxDecimalPlaces).ToString();

	AchievementProgressBar->SetPercent(Progress);
	FString TotalText =
		UKismetStringLibrary::Conv_IntToString(TotalAchieved) + " / " +
		UKismetStringLibrary::Conv_IntToString(Total) + " (" +
		ProgressString + "%)";

	ProgressText->SetText(FText::FromString(TotalText));
}

void UWidgetAchievementProgressDetails::NativePreConstruct()
{
	Title->SetText(FText::FromString(TitleText));
	AchievementProgressBar->SetFillColorAndOpacity(ProgressBarColour);
	Title->SetVisibility(TitleText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UWidgetAchievementProgressDetails::NativeConstruct()
{
	UpdateDetails();
}
