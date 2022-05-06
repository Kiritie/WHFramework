// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Step/Base/StepBase.h"
#include "Step_PlaySingleSound.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStep_PlaySingleSound : public UStepBase
{
	GENERATED_BODY()

public:
	UStep_PlaySingleSound();

public:
	/// 声音
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
	class USoundBase* Sound;
	/// 音量
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
	float Volume;
	/// 点位
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
	class ATargetPoint* Point;

public:
	virtual void OnEnter(UStepBase* InLastStep) override;

	virtual void OnLeave() override;
};
