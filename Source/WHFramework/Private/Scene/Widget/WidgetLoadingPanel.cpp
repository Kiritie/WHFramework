// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetLoadingPanel.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Scene/SceneModuleBPLibrary.h"

UWidgetLoadingPanel::UWidgetLoadingPanel(const FObjectInitializer& objectInitializer) :Super(objectInitializer)
{
	WidgetName = FName("LoadingPanel");
	WidgetType = EWidgetType::Temporary;
	InputMode = EInputMode::None;
	
	LoadingLevelPath = NAME_None;
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
}

void UWidgetLoadingPanel::OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant)
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

void UWidgetLoadingPanel::OnClose_Implementation(bool bInstant)
{
	Super::OnClose_Implementation(bInstant);
	
	FinishClose(bInstant);
}

void UWidgetLoadingPanel::NativeConstruct()
{
	Super::NativeConstruct();
}

void UWidgetLoadingPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Progress = USceneModuleBPLibrary::GetAsyncLoadLevelProgress(LoadingLevelPath);
	if(Progress > LoadProgress)
	{
		LoadProgress = Progress;
	}
	if(LoadProgress >= 0)
	{
		CurrentProgress = FMath::FInterpConstantTo(CurrentProgress, LoadProgress, InDeltaTime, 1.f);
		if(PBarProgress)
		{
			PBarProgress->SetPercent(CurrentProgress);
		}
		if(TxtProgress)
		{
			TxtProgress->SetText(FText::FromString(FString::Printf(TEXT("%d/100"), FMath::FloorToInt(CurrentProgress * 100.f))));
		}
		UE_LOG(LogTemp, Log, TEXT("Load level progress: %f"), CurrentProgress);
	}
}
