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
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

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
	/**玩家可以解锁的所有成就列表 */
	UPROPERTY(EditAnywhere, Category = "Achievements", meta=(TitleProperty = "Key"))
	TArray<FAchievementData> Achievements;

	/**如果为true, CompletionistAchievementKey将在获得所有其他成就后解锁*/
	UPROPERTY(EditAnywhere, Category = "Achievements")
	bool bUseCompletionistAchievement = false;

	/**当所有其他成就都获得时，成就的钥匙将被解锁*/
	UPROPERTY(EditAnywhere, Category = "Achievements")
	FName CompletionistAchievementKey = FName();

	/**如果是，成就解锁通知将显示在玩家屏幕上 */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowAchievementUnlocks = true;
	/**如果是，成就进度更新通知将显示在玩家屏幕上 */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowAchievementProgress = true;
	/**定义将显示进度和解锁通知的屏幕角落 */
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	EAchievementSceenLocation ScreenLocation = EAchievementSceenLocation::BOTTOM_RIGHT;
	/**任何时候屏幕上出现的成就通知总数*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	int32 TotalOnScreen = 3;
	/**一旦解锁或取得进展，成就将在屏幕上显示的秒数*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	float ScreenTime = 3;
	/**如果为真，进度通知将显示它们的确切进度，如果为假，它将显示最近的括号*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	bool bShowExactProgress = false;
	/**UI成就中可以显示的最大小数位数*/
	UPROPERTY(EditAnywhere, Category = "Achievement Unlocks")
	int32 MaxDecimalPlaces = 2;

	/**当通知从屏幕上删除时播放的动画。这可以是淡出，消失，滑动或缩小*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	EAchievementAnimation ExitAnimation = EAchievementAnimation::SLIDE;
	/**成就通知离开屏幕所需的秒数*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	float ExitAnimationLength = 0.5;
	/**成就通知进入屏幕所需的秒数*/
	UPROPERTY(EditAnywhere, Category = "Achievement Animation")
	float EntranceAnimationLength = 0.2;

	/**使用这个成就界面来显示进度并解锁玩家屏幕上的更新*/
	UPROPERTY(EditAnywhere, Category = "UI Achievement Indicator")
	TSubclassOf<UWidgetAchievement> AchievementWidgetClass;
	/**解锁成就时播放的声音会显示给用户。只在ShowAchievementUnlocks为真时播放*/
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	FSoftObjectPath UnlockSound;
	/**当进度更新显示给用户时播放的声音。只有当ShowAchievementProgress为真时才会播放*/
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	FSoftObjectPath ProgressMadeSound;

	/**当某项成就尚未完成时，附加在进度条上*/
	UPROPERTY(EditAnywhere, Category = "UI Achievement Indicator")
	FString AchievedWord = FString("(Achieved)");
	/**完成某项成就时附加在进度条上*/
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
