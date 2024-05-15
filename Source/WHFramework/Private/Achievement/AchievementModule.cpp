// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/AchievementModule.h"

#include "Achievement/Widget/WidgetAchievement.h"
#include "Achievement/Widget/WidgetAchievementHUD.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/AchievementSaveGame.h"
#include "UObject/ConstructorHelpers.h"
#include "Widget/WidgetModuleStatics.h"

IMPLEMENTATION_MODULE(UAchievementModule)

// Sets default values
UAchievementModule::UAchievementModule()
{
	ModuleName = FName("AchievementModule");
	ModuleDisplayName = FText::FromString(TEXT("Achievement Module"));
	ModuleSaveGame = UAchievementSaveGame::StaticClass();

	ModuleDependencies = { FName("AudioModule"), FName("WidgetModule") };
	
	static ConstructorHelpers::FClassFinder<UWidgetAchievement> AchievementWidgetClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Achievement/Blueprints/Widget/WBP_Achievement.WBP_Achievement_C'"));
	if(AchievementWidgetClassHelper.Succeeded())
	{
		AchievementWidgetClass = AchievementWidgetClassHelper.Class;
	}

	if(!UnlockSound.IsValid())
	{
		UnlockSound = FString("/WHFramework/Achievement/Sounds/Achievement_Unlock_Cue.Achievement_Unlock_Cue");
	}
	if(!ProgressMadeSound.IsValid())
	{
		ProgressMadeSound = FString("/WHFramework/Achievement/Sounds/Achievement_Progress_Cue.Achievement_Progress_Cue");
	}
}

UAchievementModule::~UAchievementModule()
{
	TERMINATION_MODULE(UAchievementModule)
}

#if WITH_EDITOR
void UAchievementModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAchievementModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UAchievementModule)
}
#endif

void UAchievementModule::OnInitialize()
{
	Super::OnInitialize();

	States.SetNum(Achievements.Num());
}

void UAchievementModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAchievementModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAchievementModule::OnPause()
{
	Super::OnPause();
}

void UAchievementModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAchievementModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UAchievementModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FAchievementModuleSaveData>();

	States = SaveData.States;
	TotalUnlocked = SaveData.TotalUnlocked;
}

void UAchievementModule::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);

	States.SetNum(Achievements.Num());
	TotalUnlocked = 0;
}

FSaveData* UAchievementModule::ToData()
{
	static FAchievementModuleSaveData SaveData;
	SaveData = FAchievementModuleSaveData();
	
	SaveData.States = States;
	SaveData.TotalUnlocked = TotalUnlocked;
	return &SaveData;
}

FString UAchievementModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Totle achievements: %d\nUnlocked achievements: %d"), States.Num(), GetAchievedAchievementCount());
}

bool UAchievementModule::IsAchievementExists(FName InKey) const
{
	AchievementStorageData Temp = GetAchievementLocation(InKey);
	return Temp.Index != -1;
}

int32 UAchievementModule::GetAchievedAchievementCount() const
{
	return TotalUnlocked;
}

float UAchievementModule::GetAchievedAchievementPercentage() const
{
	if (States.Num() == 0)
	{
		return 0;
	}
	return (float)GetAchievedAchievementCount() / States.Num() * 100;
}

void UAchievementModule::GetAchievementState(FName InKey, bool &OutFoundAchievement, FAchievementData &OutData, FAchievementStates& OutState) const
{
	OutFoundAchievement = false;
	AchievementStorageData i = GetAchievementLocation(InKey);
	if (i.Index == -1)
		return;

	const FAchievementData Current = i.Data;
	
	OutState = States[i.Index];
	OutData = Current;
	OutFoundAchievement = true;
}


void UAchievementModule::UnlockAchievement(FName InKey)
{
	AchievementStorageData i = GetAchievementLocation(InKey);
	if(i.Index == -1)
		return;
	
	const FAchievementData Current = i.Data;
	int32 index = i.Index;
	
	if (!States[index].Achieved)
	{
		TotalUnlocked += 1;
		States[index].Progress = Current.ProgressGoal;
		States[index].Achieved = true;
		DisplayProgress(Current, index);

		if(bUseCompletionistAchievement && TotalUnlocked == Achievements.Num() - 1)
		{
			UnlockAchievement(CompletionistAchievementKey);
		}
	}
}

void UAchievementModule::SetAchievementProgress(FName InKey, float InProgress)
{
	AchievementStorageData i = GetAchievementLocation(InKey);
	if (i.Index == -1)
		return;
	const FAchievementData Current = i.Data;
	int32 index = i.Index;
	
	if (Current.Progress)
	{
		if (InProgress >= Current.ProgressGoal)
		{
			UnlockAchievement(InKey);
		}
		else
		{
			States[index].Progress = InProgress;
			DisplayProgress(Current, index);
		}
	}
}

void UAchievementModule::AddAchievementProgress(FName InKey, float InProgress)
{
	AchievementStorageData i = GetAchievementLocation(InKey);
	if (i.Index == -1)
		return;
	
	const FAchievementData Current = i.Data;
	int32 index = i.Index;

	if (Current.Progress)
	{
		if (States[index].Progress + InProgress >= Current.ProgressGoal)
		{
			UnlockAchievement(InKey);
		}
		else
		{
			States[index].Progress += InProgress;
			DisplayProgress(Current, index);
		}
	}
}

void UAchievementModule::DisplayProgress(FAchievementData InAchievement, int32 InIndex)
{
	if((bShowAchievementUnlocks && States[InIndex].Achieved) || (bShowAchievementProgress && !States[InIndex].Achieved))
	{
		if (!InAchievement.Spoiler || States[InIndex].Achieved)
		{
			if (UWidgetAchievementHUD* HUD = UWidgetModuleStatics::GetUserWidget<UWidgetAchievementHUD>())
			{
				if (InAchievement.Progress && States[InIndex].Progress < InAchievement.ProgressGoal)
				{
					if(InAchievement.NotificationFrequency > 0)
					{
						int32 Steps = (int32)InAchievement.ProgressGoal / (int32)InAchievement.NotificationFrequency;

						//Loop from biggest notification bracket to the smallest
						for (int32 i = Steps; i > States[InIndex].LastProgressUpdate; i--)
						{
							if (States[InIndex].Progress >= InAchievement.NotificationFrequency * i)
							{
								States[InIndex].LastProgressUpdate = i;
								HUD->ScheduleAchievementDisplay(InAchievement, States[InIndex]);
								return;
							}
						}
					}
				}
				else
				{
					HUD->ScheduleAchievementDisplay(InAchievement, States[InIndex]);
				}
			}
		}
	}
}

AchievementStorageData UAchievementModule::GetAchievementLocation(FName InKey) const
{
	AchievementStorageData location = AchievementStorageData();
	for(int32 i = 0; i < Achievements.Num(); i++)
	{
		if(Achievements[i].Key == InKey)
		{
			location.Data = Achievements[i];
			location.Index = i;
		}
	}
	return location;
}

bool UAchievementModule::IsRightOfScreen() const
{
	return ScreenLocation == EAchievementSceenLocation::TOP_RIGHT || ScreenLocation == EAchievementSceenLocation::BOTTOM_RIGHT;
}

bool UAchievementModule::IsBottomOfScreen() const
{
	return ScreenLocation == EAchievementSceenLocation::BOTTOM_LEFT || ScreenLocation == EAchievementSceenLocation::BOTTOM_RIGHT;
}

TArray<TSharedPtr<FName>> UAchievementModule::GetComboBoxNames()
{
	TArray<TSharedPtr<FName>> NameList;
	for(FAchievementData i : Achievements)
	{
		if(CompletionistAchievementKey != i.Key)
		{
			TSharedPtr<FName> NameKey = MakeShareable(new FName(i.Key));
			NameList.Add(NameKey);
		}
	}
	return NameList;
}