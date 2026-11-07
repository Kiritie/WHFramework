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

	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	const int32 TotalAchieved = Module ? Module->GetAchievedAchievementCount() : 0;
	const int32 Total = Module ? Module->Achievements.Num() : 0;
	const float Progress = Total > 0 ? static_cast<float>(TotalAchieved) / Total : 0.f;

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
