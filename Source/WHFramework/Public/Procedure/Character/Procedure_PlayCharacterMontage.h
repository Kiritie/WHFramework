// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayCharacterMontage.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedure_PlayCharacterMontage : public UProcedureBase
{
	GENERATED_BODY()

public:
	UProcedure_PlayCharacterMontage();

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
	FName CharacterName;
	/// 蒙太奇
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	class UAnimMontage* Montage;
};
