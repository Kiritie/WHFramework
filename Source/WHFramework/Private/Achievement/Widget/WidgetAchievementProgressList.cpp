// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Widget/WidgetAchievementProgressList.h"

#include "Achievement/AchievementModule.h"
#include "Achievement/AchievementModuleTypes.h"
#include "Achievement/Widget/WidgetAchievement.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/GameplayStatics.h"

UWidgetAchievementProgressList::UWidgetAchievementProgressList(const FObjectInitializer& ObjectInitializer) : UUserWidget(ObjectInitializer)
{
}

void UWidgetAchievementProgressList::UpdateList()
{
	TArray<FAchievementStates> States = UAchievementModule::Get()->States;
	TArray<FAchievementData> Data = UAchievementModule::Get()->Achievements;
	int32 InList = 0;
	
	ListView->ClearChildren();
	for(int i = 0; i < Data.Num(); i++)
	{
		if(	Filter == EAchievementProgressFilter::ANY || 
			Filter == EAchievementProgressFilter::ACHIEVED && States[i].Achieved ||
			Filter == EAchievementProgressFilter::UNACHIEVED && !States[i].Achieved
		)
		{
			if(!Data[i].Spoiler || States[i].Achieved)
			{
				UWidgetAchievement* Temp = CreateWidget<UWidgetAchievement>(UGameplayStatics::GetPlayerController(GetWorld(), 0), UAchievementModule::Get()->AchievementWidgetClass);
				Temp->SetValue(UAchievementModule::Get()->Achievements[i], States[i], true);
				ListView->AddChild(Temp);
			}
			else
			{
				InList++;
			}
		}
	}

	HiddenText->SetVisibility(bShowHiddenAchievementCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if(bShowHiddenAchievementCount)
	{
		FString Text = UKismetStringLibrary::Conv_IntToString(InList) + " Hidden Achievement" + (InList > 1? "s" : "");
		HiddenText->SetText(FText::FromString(Text));
	}
}

void UWidgetAchievementProgressList::NativePreConstruct()
{
	HiddenText->SetVisibility(bShowHiddenAchievementCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UWidgetAchievementProgressList::NativeConstruct()
{
	UpdateList();
}