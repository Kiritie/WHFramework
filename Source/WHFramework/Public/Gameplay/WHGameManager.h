// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "WHGameManager.generated.h"

/**
 * 游戏管理器基类
 */
UCLASS()
class WHFRAMEWORK_API AWHGameManager : public AWHActor
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AWHGameManager();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ManagerStats")
	FName ManagerName;

public:
	UFUNCTION(BlueprintPure)
	FName GetManagerName() const { return ManagerName; }
};
