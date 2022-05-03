// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ProcedureModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "ProcedureModule.generated.h"

class UProcedureBase;
class URootProcedureBase;

/**
 * 
 */

UENUM(BlueprintType)
enum class EProcedureModuleState : uint8
{
	None,
	Running,
	Ended
};

UCLASS()
class WHFRAMEWORK_API AProcedureModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	AProcedureModule();

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
	/// ProcedureModule
protected:
	/// 自动开始流程
	UPROPERTY(EditAnywhere, Category = "ProcedureModule")
	bool bAutoStartProcedure;

public:
	UFUNCTION(BlueprintCallable)
	void StartProcedure(int32 InRootProcedureIndex = -1, bool bSkipProcedures = false);

	UFUNCTION(BlueprintCallable)
	void EndProcedure(bool bRestoreProcedures = false);

	UFUNCTION(BlueprintCallable)
	void RestoreProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void EnterProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void RefreshProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void GuideProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void ExecuteProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable)
	void LeaveProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void ClearAllProcedure();

public:
	UFUNCTION(BlueprintPure)
	bool IsAllProcedureCompleted();

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);

	void SetRootProcedureItem(int32 InIndex, URootProcedureBase* InRootProcedure);
	#endif

	//////////////////////////////////////////////////////////////////////////
	/// Root Procedure
protected:
	/// 初始根流程索引
	UPROPERTY(EditAnywhere, Category = "ProcedureModule|Root")
	int32 FirstRootProcedureIndex;
	/// 当前根流程索引
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Root")
	int32 CurrentRootProcedureIndex;
	/// 根流程
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Root")
	TArray<URootProcedureBase*> RootProcedures;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentRootProcedureIndex() const { return CurrentRootProcedureIndex; }

	/**
	* 获取当前根流程
	*/
	UFUNCTION(BlueprintPure)
	URootProcedureBase* GetCurrentRootProcedure() const
	{
		if(RootProcedures.IsValidIndex(CurrentRootProcedureIndex))
		{
			return RootProcedures[CurrentRootProcedureIndex];
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure)
	TArray<URootProcedureBase*>& GetRootProcedures() { return RootProcedures; }

	//////////////////////////////////////////////////////////////////////////
	/// Current Procedure
protected:
	/// 当前流程 
	UPROPERTY(VisibleAnywhere, Transient, Category = "ProcedureModule|Stats")
	UProcedureBase* CurrentProcedure;

	/// 流程模块状态
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Stats")
	EProcedureModuleState ProcedureModuleState;

public:
	UFUNCTION(BlueprintPure)
	EProcedureModuleState GetProcedureModuleState() const { return ProcedureModuleState; }
	/**
	* 获取当前流程
	*/
	UFUNCTION(BlueprintPure)
	UProcedureBase* GetCurrentProcedure() const { return CurrentProcedure; }

	//////////////////////////////////////////////////////////////////////////
	/// Global Options
protected:
	/// 流程执行方式
	UPROPERTY(EditAnywhere, Category = "ProcedureModule|Global Options")
	EProcedureExecuteType GlobalProcedureExecuteType;
	/// 流程完成方式
	UPROPERTY(EditAnywhere, Category = "ProcedureModule|Global Options")
	EProcedureCompleteType GlobalProcedureCompleteType;
	/// 流程离开方式
	UPROPERTY(EditAnywhere, Category = "ProcedureModule|Global Options")
	EProcedureLeaveType GlobalProcedureLeaveType;

public:
	UFUNCTION(BlueprintPure)
	EProcedureExecuteType GetGlobalProcedureExecuteType() const { return GlobalProcedureExecuteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalProcedureExecuteType(EProcedureExecuteType InGlobalProcedureExecuteType);

	UFUNCTION(BlueprintPure)
	EProcedureCompleteType GetGlobalProcedureCompleteType() const { return GlobalProcedureCompleteType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalProcedureCompleteType(EProcedureCompleteType InGlobalProcedureCompleteType);

	UFUNCTION(BlueprintPure)
	EProcedureLeaveType GetGlobalProcedureLeaveType() const { return GlobalProcedureLeaveType; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalProcedureLeaveType(EProcedureLeaveType InGlobalProcedureLeaveType);
};
