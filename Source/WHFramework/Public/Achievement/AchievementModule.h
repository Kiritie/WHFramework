// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AchievementModuleTypes.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"

#include "AchievementModule.generated.h"

class UWidgetAchievement;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAchievementModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(UAchievementModule)

public:
	// ParamSets default values for this actor's properties
	UAchievementModule();

	~UAchievementModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	UFUNCTION(BlueprintPure, Category="Achievements")
	bool IsAchievementExists(FName InKey) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetAchievedAchievementCount() const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	float GetAchievedAchievementPercentage() const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	void GetAchievementState(FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState) const;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void UnlockAchievement(FName InKey);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void SetAchievementProgress(FName InKey, float InProgress);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void AddAchievementProgress(FName InKey, float InProgress);

public:
	/**List of all achievement which can be unlocked by the player */
	UPROPERTY(EditAnywhere, Category = "Achievements", meta=(TitleProperty = "Key"))
	TArray<FAchievementData> Achievements;

	/**If true, CompletionistAchievementKey will be unlocked when all other achievement have be gained*/
	UPROPERTY(EditAnywhere, Category = "Achievements")
	bool bUseCompletionistAchievement = false;

	/**The key of the achievement to be unlocked when all others have be gained*/
	UPROPERTY(EditAnywhere, Category = "Achievements")
	FName CompletionistAchievementKey = FName();

	/**If true, achievement unlock notifications will be displayed on the players screen. */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowAchievementUnlocks = true;
	/**If true, achievement progress update notifications will be displayed on the players screen. */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowAchievementProgress = true;
	/**Define the corner of the screen that progress and unlock notifications will be displayed. */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	EAchievementSceenLocation ScreenLocation = EAchievementSceenLocation::BOTTOM_RIGHT;
	/**The total number of achievement notifications which can be on the screen at any one time.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	int32 TotalOnScreen = 3;
	/**The number of seconds an achievement will display on the screen once unlocked or progress is made.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	float ScreenTime = 3;
	/**If true, progress notifications will display their exact progress, if false it will show the closest bracket.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowExactProgress = false;
	/**The max number of decimal places which can be displayed on a UI Achievement*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	int32 MaxDecimalPlaces = 2;

	/**The animation that is played when notifications are removed from the screen. This can either be Fade, Disappear, Slide or Scale Down.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	EAchievementAnimation ExitAnimation = EAchievementAnimation::SLIDE;
	/**The number of seconds an achievement notification takes to leave the screen.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	float ExitAnimationLength = 0.5;
	/**The number of seconds an achievement notification takes to enter the screen.*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	float EntranceAnimationLength = 0.2;

	/**Use this achievement ui to display progress and unlock updates to the players screen*/
	UPROPERTY(EditAnywhere, Category = "UI Achievement Indicator")
	TSubclassOf<UWidgetAchievement> AchievementWidgetClass;
	/**The sound that plays when an achievement is unlocked is displayed to a user. Only plays if ShowAchievementUnlocks is true.*/
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	FSoftObjectPath UnlockSound;
	/**The sound that plays when a progress update is displayed to a user. Only plays if ShowAchievementProgress is true.*/
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	FSoftObjectPath ProgressMadeSound;

	/**Appends to the progress bar when an achievement has not been achieved*/
	UPROPERTY(EditAnywhere, Category = "UI Achievement Indicator")
	FString AchievedWord = FString("(Achieved)");
	/**Appends to the progress bar when an achievement is achieved*/
	UPROPERTY(EditAnywhere, Category = "UI Achievement Indicator")
	FString UnAchievedWord = FString("(Locked)");

	UPROPERTY()
	TArray<FAchievementStates> States;
	
	UPROPERTY()
	int32 TotalUnlocked;
	
public:
	AchievementStorageData GetAchievementLocation(FName InKey) const;

	bool IsRightOfScreen() const;

	bool IsBottomOfScreen() const;

	TArray<TSharedPtr<FName>> GetComboBoxNames();

protected:
	void DisplayProgress(FAchievementData InAchievement, int32 InIndex);
};
