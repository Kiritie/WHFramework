// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetLoadingLevelPanel.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/Handle/Common/Game/EventHandle_GameExited.h"
#include "Scene/SceneModuleStatics.h"

UWidgetLoadingLevelPanel::UWidgetLoadingLevelPanel(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	WidgetName = FName("LoadingLevelPanel");
	WidgetType = EWidgetType::Temporary;
	WidgetRefreshType = EWidgetRefreshType::Tick;
	WidgetInputMode = EInputMode::None;
	
	LevelPath = NAME_None;
	bUnloading = false;
	LoadProgress = 0.f;
	CurrentProgress = 0.f;
}

void UWidgetLoadingLevelPanel::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	
	LoadProgress = 0.f;
	CurrentProgress = 0.f;

	if(InParams.IsValidIndex(0))
	{
		LevelPath = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		bUnloading = InParams[1];
	}
}

void UWidgetLoadingLevelPanel::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetLoadingLevelPanel::OnRefresh()
{
	Super::OnRefresh();

	if(!LevelPath.IsNone())
	{
		const float Progress = bUnloading ? USceneModuleStatics::GetAsyncUnloadLevelProgress(LevelPath) : (USceneModuleStatics::GetAsyncLoadLevelProgress(LevelPath) * 0.5f);
		if(Progress > LoadProgress)
		{
			LoadProgress = Progress;
		}
	}
	if(LoadProgress >= 0)
	{
		if(!FMath::IsNearlyEqual(CurrentProgress, LoadProgress))
		{
			CurrentProgress = FMath::FInterpConstantTo(CurrentProgress, LoadProgress, GetWorld()->GetDeltaSeconds(), 1.f);
		}
		if(PBar_Progress)
		{
			PBar_Progress->SetPercent(CurrentProgress);
		}
		if(Txt_Progress)
		{
			Txt_Progress->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(CurrentProgress * 100.f))));
		}
		WHLog(FString::Printf(TEXT("Load level progress: %f"), CurrentProgress));
	}
}

float UWidgetLoadingLevelPanel::GetLoadProgress(bool bSmoothness) const
{
	return bSmoothness ? CurrentProgress : LoadProgress;
}

void UWidgetLoadingLevelPanel::SetLoadProgress(float InLoadProgress, bool bSmoothness)
{
	InLoadProgress = FMath::Clamp(InLoadProgress, 0.f, 1.f);
	LoadProgress = InLoadProgress;
	if(!bSmoothness)
	{
		CurrentProgress = LoadProgress;
	}
}
