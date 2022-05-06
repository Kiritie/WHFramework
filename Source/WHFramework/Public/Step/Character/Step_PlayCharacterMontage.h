// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Step/Base/StepBase.h"
#include "Step_PlayCharacterMontage.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStep_PlayCharacterMontage : public UStepBase
{
	GENERATED_BODY()

public:
	UStep_PlayCharacterMontage();

	//////////////////////////////////////////////////////////////////////////
	/// Step
public:
	virtual void OnEnter(UStepBase* InLastStep) override;

	virtual void OnLeave() override;

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	FName CharacterName;
	/// 蒙太奇
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	class UAnimMontage* Montage;
};
