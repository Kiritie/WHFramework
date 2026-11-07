// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Achievement/AchievementModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AchievementModuleStatics.generated.h"
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAchievementModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//Miscellaneous
	/**Returns true if an achievement is found in the list.*/
	UFUNCTION(BlueprintPure, Category="AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static bool IsAchievementExists(const UObject* WorldContextObject, FName InKey);

	/**Returns the total number of achievements which have been unlocked.*/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static int32 GetAchievedAchievementCount(const UObject* WorldContextObject);

	/**Returns the current percentage of unlocked achievements.*/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static float GetAchievedAchievementPercentage(const UObject* WorldContextObject);

	/***/
	UFUNCTION(BlueprintPure, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void GetAchievementState(const UObject* WorldContextObject, FName InKey, bool& OutFoundAchievement, FAchievementData& OutData, FAchievementStates& OutState);

	//Unlocking/Progress
	/**Fully unlocks a progress or goal achievement.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void UnlockAchievement(const UObject* WorldContextObject, FName InKey);

	/**Set the progress of an achievement to a specific value.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void SetAchievementProgress(const UObject* WorldContextObject, FName InKey, float InProgress);

	/**Adds the input amount of progress to an achievement. Clamps achievement progress to its max value.*/
	UFUNCTION(BlueprintCallable, Category = "AchievementModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void AddAchievementProgress(const UObject* WorldContextObject, FName InKey, float InProgress);
};
