// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayCharacterSound.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AProcedure_PlayCharacterSound : public AProcedureBase
{
	GENERATED_BODY()

public:
	AProcedure_PlayCharacterSound();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
	virtual void ServerOnEnter_Implementation(AProcedureBase* InLastProcedure) override;

	virtual void ServerOnLeave_Implementation(AProcedureBase* InNextProcedure) override;

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	FName CharacterName;
	/// 声音
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	class USoundBase* Sound;
};
