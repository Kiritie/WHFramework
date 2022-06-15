// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetLoadingLevelPanel.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Scene/SceneModuleBPLibrary.h"

UWidgetLoadingLevelPanel::UWidgetLoadingLevelPanel(const FObjectInitializer& objectInitializer) :Super(objectInitializer)
{
	WidgetName = FName("LoadingLevelPanel");
	WidgetCategory = EWidgetCategory::Temporary;
	WidgetRefreshType = EWidgetRefreshType::Tick;
	InputMode = EInputMode::None;
	
	LoadingLevelPath = NAME_None;
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
}

void UWidgetLoadingLevelPanel::OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen_Implementation(InParams, bInstant);
	if(InParams.IsValidIndex(0))
	{
		LoadingLevelPath = *InParams[0].GetStringValue();
	}
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
	FinishOpen(bInstant);
}

void UWidgetLoadingLevelPanel::OnClose_Implementation(bool bInstant)
{
	Super::OnClose_Implementation(bInstant);
	
	FinishClose(bInstant);
}

void UWidgetLoadingLevelPanel::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();

	const float Progress = USceneModuleBPLibrary::GetAsyncLoadLevelProgress(LoadingLevelPath);
	if(Progress > LoadProgress)
	{
		LoadProgress = Progress;
	}
	if(LoadProgress >= 0)
	{
		CurrentProgress = FMath::FInterpConstantTo(CurrentProgress, LoadProgress, GetWorld()->GetDeltaSeconds(), 1.f);
		if(ProgressBar_Progress)
		{
			ProgressBar_Progress->SetPercent(CurrentProgress);
		}
		if(TextBlock_Progress)
		{
			TextBlock_Progress->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(CurrentProgress * 100.f))));
		}
		UE_LOG(LogTemp, Log, TEXT("Load level progress: %f"), CurrentProgress);
	}
}
