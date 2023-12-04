// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "StepModuleTypes.h"
#include "Base/StepAsset.h"
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

public:
	UFUNCTION(BlueprintPure)
	bool IsAllStepCompleted();

	//////////////////////////////////////////////////////////////////////////
	/// Step Stats
protected:
	/// 默认流程资产 
	UPROPERTY(EditAnywhere, Category = "StepModule|Step Stats")
	UStepAsset* DefaultAsset;
	/// 当前流程资产 
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	UStepAsset* CurrentAsset;
	/// 当前步骤 
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	UStepBase* CurrentStep;
	/// 当前根步骤索引
	UPROPERTY(VisibleAnywhere, Category = "StepModule|Step Stats")
	int32 CurrentRootStepIndex;

public:
	/**
	* 获取默认资产
	*/
	UFUNCTION(BlueprintPure)
	UStepAsset* GetDefaultAsset() const { return DefaultAsset; }
	/**
	* 获取当前资产
	*/
	UFUNCTION(BlueprintPure)
	UStepAsset* GetCurrentAsset() const { return CurrentAsset; }
	/**
	* 设置当前资产
	*/
	UFUNCTION(BlueprintCallable)
	void SetCurrentAsset(UStepAsset* InStepAsset, bool bInAutoStartFirst = false);
	/**
	* 获取初始步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetFirstStep() const { return CurrentAsset ? CurrentAsset->FirstStep : nullptr; }
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
		if(GetRootSteps().IsValidIndex(CurrentRootStepIndex))
		{
			return GetRootSteps()[CurrentRootStepIndex];
		}
		return nullptr;
	}
	/**
	* 获取根步骤列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UStepBase*> GetRootSteps() const { return CurrentAsset ? CurrentAsset->RootSteps : TArray<UStepBase*>(); }
	/**
	* 获取步骤Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<FString, UStepBase*> GetStepMap() const { return CurrentAsset ? CurrentAsset->StepMap : TMap<FString, UStepBase*>(); }
	/**
	* 通过GUID获取步骤
	*/
	UFUNCTION(BlueprintPure)
	UStepBase* GetStepByGUID(const FString& InStepGUID) const { return *GetStepMap().Find(InStepGUID); }

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
