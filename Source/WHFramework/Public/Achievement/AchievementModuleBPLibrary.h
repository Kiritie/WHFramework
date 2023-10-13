// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Kismet/BlueprintFunctionLibrary.h"
#include "AchievementModuleBPLibrary.generated.h"
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAchievementModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//Miscellaneous
	/**Returns true if an achievement is found in the list.*/
	UFUNCTION(BlueprintPure, Category="AchievementModuleBPLibrary")
	static bool IsAchievementExists(FName InKey);

	/**Returns the total number of achievements which have been unlocked.*/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleBPLibrary")
	static int32 GetAchievedAchievementCount();

	/**Returns the current percentage of unlocked achievements.*/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleBPLibrary")
	static float GetAchievedAchievementPercentage();

	/***/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleBPLibrary")
	static void GetAchievementState(FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState);

	//Unlocking/Progress
	/**Fully unlocks a progress or goal achievement.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleBPLibrary")
	static void UnlockAchievement(FName InKey);

	/**Set the progress of an achievement to a specific value.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleBPLibrary")
	static void SetAchievementProgress(FName InKey, float InProgress);

	/**Adds the input amount of progress to an achievement. Clamps achievement progress to its max value.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleBPLibrary")
	static void AddAchievementProgress(FName InKey, float InProgress);
};
