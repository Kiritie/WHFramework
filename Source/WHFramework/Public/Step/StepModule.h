// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "StepModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "StepModule.generated.h"

class UStepBase;

/**
 * 
 */

UENUM(BlueprintType)
enum class EStepModuleState : uint8
{
	None,
	Running,
	Paused,
	Ended
};

UCLASS()
class WHFRAMEWORK_API UStepModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UStepModule)

public:
	// ParamSets default values for this actor's properties
	UStepModule();

	~UStepModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// StepModule
protected:
	/// 自动开始初始步骤
	UPROPERTY(EditAnywhere, Category = "StepModule")
	bool bAutoStartFirst;
	
	/// 步骤模块状态
	UPROPERTY(VisibleAnywhere, Category = "StepModule")
	EStepModuleState StepModuleState;

public:
	/**
	* 获取步骤模块状态
	*/
	UFUNCTION(BlueprintPure)
	EStepModuleState GetStepModuleState() const { return StepModuleState; }

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
	/// Step Stats
protected:
	/// 初始步骤 
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	UStepBase* FirstStep;
	/// 当前步骤 
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	UStepBase* CurrentStep;
	/// 当前根步骤索引
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	int32 CurrentRootStepIndex;
	/// 根步骤
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	TArray<UStepBase*> RootSteps;
	/// 步骤Map
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	TMap<FString, UStepBase*> StepMap;

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
	* 获取当前根步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetCurrentRootStep() const
	{
		if(RootSteps.IsValidIndex(CurrentRootStepIndex))
		{
			return RootSteps[CurrentRootStepIndex];
		}
		return nullptr;
	}
	/**
	* 获取根步骤列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UStepBase*>& GetRootSteps() { return RootSteps; }
	/**
	* 获取步骤Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UStepBase*>& GetStepMap() { return StepMap; }
	/**
	* 通过GUID获取步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetStepByGUID(const FString& InStepGUID) const { return *StepMap.Find(InStepGUID); }

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

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FStepListItem>>& OutStepListItems);
	#endif
};
