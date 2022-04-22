// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayCharacterSound.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedure_PlayCharacterSound : public UProcedureBase
{
	GENERATED_BODY()

public:
	UProcedure_PlayCharacterSound();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
	virtual void OnEnter(UProcedureBase* InLastProcedure) override;

	virtual void OnLeave() override;

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	FString CharacterName;
	/// 声音
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	class USoundBase* Sound;
};
