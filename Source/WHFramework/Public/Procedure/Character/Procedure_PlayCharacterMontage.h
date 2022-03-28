// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayCharacterMontage.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AProcedure_PlayCharacterMontage : public AProcedureBase
{
	GENERATED_BODY()

public:
	AProcedure_PlayCharacterMontage();

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
	FString CharacterName;
	/// 蒙太奇
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	class UAnimMontage* Montage;
};
