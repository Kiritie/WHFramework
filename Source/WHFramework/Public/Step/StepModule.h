// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "StepModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "StepModule.generated.h"

class UStepBase;
class URootStepBase;

/**
 * 
 */

UENUM(BlueprintType)
enum class EStepModuleState : uint8
{
	None,
	Running,
	Ended
};

UCLASS()
class WHFRAMEWORK_API AStepModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	AStepModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
	#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// StepModule
protected:
	/// 自动开始步骤
	UPROPERTY(EditAnywhere, Category = "StepModule")
	bool bAutoStartStep;

public:
	UFUNCTION(BlueprintCallable)
	void StartStep(int32 InRootStepIndex = -1, bool bSkipSteps = false);

	UFUNCTION(BlueprintCallable)
	void EndStep(bool bRestoreSteps = false);

	UFUNCTION(BlueprintCallable)
	void RestoreStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void EnterStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void RefreshStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void GuideStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void ExecuteStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult = EStepExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void LeaveStep(UStepBase* InStep);

	UFUNCTION(BlueprintCallable)
	void ClearAllStep();

public:
	UFUNCTION(BlueprintPure)
	bool IsAllStepCompleted();

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	void SetRootStepItem(int32 InIndex, URootStepBase* InRootStep);
	#endif

	//////////////////////////////////////////////////////////////////////////
	/// Root Step
protected:
	/// 当前根步骤索引
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Root Step")
	int32 CurrentRootStepIndex;
	/// 根步骤
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Root Step")
	TArray<URootStepBase*> RootSteps;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentRootStepIndex() const { return CurrentRootStepIndex; }

	/**
	* 获取当前根步骤
	*/
	UFUNCTION(BlueprintPure)
	URootStepBase* GetCurrentRootStep() const
	{
		if(RootSteps.IsValidIndex(CurrentRootStepIndex))
		{
			return RootSteps[CurrentRootStepIndex];
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure)
	TArray<URootStepBase*>& GetRootSteps() { return RootSteps; }

	//////////////////////////////////////////////////////////////////////////
	/// Step Stats
protected:
	/// 初始步骤 
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	UStepBase* FirstStep;
	
	/// 当前步骤 
	UPROPERTY(VisibleAnywhere, Transient, Category = "StepModule|Step Stats")
	UStepBase* CurrentStep;

	/// 步骤模块状态
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	EStepModuleState StepModuleState;

public:
	/**
	* 获取初始步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetFirstStep() const { return FirstStep; }
	/**
	* 设置初始步骤
	*/
	UFUNCTION(BlueprintCallable)
	void SetFirstStep(UStepBase* InFirstStep) { this->FirstStep = InFirstStep; }
	/**
	* 获取当前步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetCurrentStep() const { return CurrentStep; }
	/**
	* 获取步骤模块状态
	*/
	UFUNCTION(BlueprintPure)
	EStepModuleState GetStepModuleState() const { return StepModuleState; }

	//////////////////////////////////////////////////////////////////////////
	/// Global Options
protected:
	/// 步骤执行方式
	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepExecuteType GlobalStepExecuteType;
	/// 步骤完成方式
	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepCompleteType GlobalStepCompleteType;
	/// 步骤离开方式
	UPROPERTY(EditAnywhere, Category = "StepModule|Global Options")
	EStepLeaveType GlobalStepLeaveType;

public:
	UFUNCTION(BlueprintPure)
	EStepExecuteType GetGlobalStepExecuteType() const { return GlobalStepExecuteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepExecuteType(EStepExecuteType InGlobalStepExecuteType);

	UFUNCTION(BlueprintPure)
	EStepCompleteType GetGlobalStepCompleteType() const { return GlobalStepCompleteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepCompleteType(EStepCompleteType InGlobalStepCompleteType);

	UFUNCTION(BlueprintPure)
	EStepLeaveType GetGlobalStepLeaveType() const { return GlobalStepLeaveType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalStepLeaveType(EStepLeaveType InGlobalStepLeaveType);
};
