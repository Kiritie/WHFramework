// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetLoadingLevelPanel.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Debug/DebugModuleTypes.h"
#include "Scene/SceneModuleBPLibrary.h"

UWidgetLoadingLevelPanel::UWidgetLoadingLevelPanel(const FObjectInitializer& objectInitializer) :Super(objectInitializer)
{
	WidgetName = FName("LoadingLevelPanel");
	WidgetType = EWidgetType::Temporary;
	WidgetRefreshType = EWidgetRefreshType::Tick;
	WidgetInputMode = EInputMode::None;
	
	LoadingLevelPath = NAME_None;
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
}

void UWidgetLoadingLevelPanel::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	if(InParams.IsValidIndex(0))
	{
		LoadingLevelPath = *InParams[0].GetStringValue();
	}
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
	FinishOpen(bInstant);
}

void UWidgetLoadingLevelPanel::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
	
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
		WHLog(FString::Printf(TEXT("Load level progress: %f"), CurrentProgress));
	}
}
