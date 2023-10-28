// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/AchievementModule.h"

#include "Achievement/Widget/WidgetAchievement.h"
#include "Achievement/Widget/WidgetAchievementHUD.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Module/AchievementSaveGame.h"
#include "UObject/ConstructorHelpers.h"
#include "Widget/WidgetModuleBPLibrary.h"

IMPLEMENTATION_MODULE(AAchievementModule)

// Sets default values
AAchievementModule::AAchievementModule()
{
	ModuleName = FName("AchievementModule");
	ModuleSaveGame = UAchievementSaveGame::StaticClass();
	
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

AAchievementModule::~AAchievementModule()
{
	TERMINATION_MODULE(AAchievementModule)
}

#if WITH_EDITOR
void AAchievementModule::OnGenerate()
{
	Super::OnGenerate();
}

void AAchievementModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AAchievementModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	States.SetNum(Achievements.Num());
}

void AAchievementModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
	
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}
}

void AAchievementModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAchievementModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAchievementModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAchievementModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void AAchievementModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FAchievementModuleSaveData>();

	States = SaveData.States;
	TotalUnlocked = SaveData.TotalUnlocked;
}

void AAchievementModule::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);

	States.SetNum(Achievements.Num());
	TotalUnlocked = 0;
}

FSaveData* AAchievementModule::ToData()
{
	static FAchievementModuleSaveData SaveData;
	SaveData = FAchievementModuleSaveData();
	
	SaveData.States = States;
	SaveData.TotalUnlocked = TotalUnlocked;
	return &SaveData;
}

bool AAchievementModule::IsAchievementExists(FName InKey) const
{
	AchievementStorageData Temp = GetAchievementLocation(InKey);
	return Temp.Index != -1;
}

int32 AAchievementModule::GetAchievedAchievementCount() const
{
	return TotalUnlocked;
}

float AAchievementModule::GetAchievedAchievementPercentage() const
{
	if (States.Num() == 0)
	{
		return 0;
	}
	return (float)GetAchievedAchievementCount() / States.Num() * 100;
}

void AAchievementModule::GetAchievementState(FName InKey, bool &OutFoundAchievement, FAchievementData &OutData, FAchievementStates& OutState) const
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


void AAchievementModule::UnlockAchievement(FName InKey)
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

void AAchievementModule::SetAchievementProgress(FName InKey, float InProgress)
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

void AAchievementModule::AddAchievementProgress(FName InKey, float InProgress)
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

void AAchievementModule::DisplayProgress(FAchievementData InAchievement, int32 InIndex)
{
	if((bShowAchievementUnlocks && States[InIndex].Achieved) || (bShowAchievementProgress && !States[InIndex].Achieved))
	{
		if (!InAchievement.Spoiler || States[InIndex].Achieved)
		{
			if (UWidgetAchievementHUD* HUD = UWidgetModuleBPLibrary::GetUserWidget<UWidgetAchievementHUD>())
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

AchievementStorageData AAchievementModule::GetAchievementLocation(FName InKey) const
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

bool AAchievementModule::IsRightOfScreen() const
{
	return ScreenLocation == EAchievementSceenLocation::TOP_RIGHT || ScreenLocation == EAchievementSceenLocation::BOTTOM_RIGHT;
}

bool AAchievementModule::IsBottomOfScreen() const
{
	return ScreenLocation == EAchievementSceenLocation::BOTTOM_LEFT || ScreenLocation == EAchievementSceenLocation::BOTTOM_RIGHT;
}

TArray<TSharedPtr<FName>> AAchievementModule::GetComboBoxNames()
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