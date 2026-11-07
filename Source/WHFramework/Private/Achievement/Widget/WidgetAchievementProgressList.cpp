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
	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	APlayerController* Player = GetOwningPlayer();
	if (!Module || !Player) return;
	const TArray<FAchievementStates>& States = Module->States;
	const TArray<FAchievementData>& Data = Module->Achievements;
	int32 InList = 0;
	
	ListView->ClearChildren();
	for(int i = 0; i < Data.Num() && i < States.Num(); i++)
	{
		if(	Filter == EAchievementProgressFilter::ANY || 
			Filter == EAchievementProgressFilter::ACHIEVED && States[i].Achieved ||
			Filter == EAchievementProgressFilter::UNACHIEVED && !States[i].Achieved
		)
		{
			if(!Data[i].Spoiler || States[i].Achieved)
			{
				if (UWidgetAchievement* Temp = CreateWidget<UWidgetAchievement>(Player, Module->AchievementWidgetClass))
				{
					Temp->SetValue(Data[i], States[i], true);
					ListView->AddChild(Temp);
				}
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
