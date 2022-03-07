// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ProcedureModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "ProcedureModule.generated.h"

class AProcedureBase;
class ARootProcedureBase;

/**
 * 
 */

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

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
	UFUNCTION(BlueprintCallable)
	void ServerStartProcedure();

	UFUNCTION(BlueprintCallable)
	void ServerEnterProcedure(AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void ServerLeaveProcedure(AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void LocalEnterProcedure(AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void LocalLeaveProcedure(AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void ServerSkipProcedure(AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void ServerSkipCurrentProcedure();

	UFUNCTION(BlueprintCallable)
	void ServerSkipCurrentParentProcedure();

	UFUNCTION(BlueprintCallable)
	void ServerSkipCurrentRootProcedure();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure/Menu
protected:
	bool bAllProcedureSpawned;
	
public:
	UFUNCTION()
	void SpawnAllProcedure();

	UFUNCTION(CallInEditor, Category = "ProcedureModule")
	void ClearAllProcedure();

	//////////////////////////////////////////////////////////////////////////
	/// Editor
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "ProcedureEditor")
	void OpenProcedureEditor();

	void GenerateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);
#endif

	//////////////////////////////////////////////////////////////////////////
	/// RootProcedure
public:
	/// 当前根流程索引
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated)
	int32 RootProcedureIndex;
	/// 根流程
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated)
	TArray<ARootProcedureBase*> RootProcedures;
public:
	/**
	 * 是否有根流程
	 */
	UFUNCTION(BlueprintPure)
	bool HasRootProcedure() const;
	/**
	* 获取当前根流程
	*/
	UFUNCTION(BlueprintPure)
	ARootProcedureBase* GetCurrentRootProcedure() const;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
protected:
	/// 当前流程 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated)
	AProcedureBase* CurrentProcedure;
	/// 当前本地流程 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated)
	AProcedureBase* CurrentLocalProcedure;
	/// 当前父流程
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated)
	AProcedureBase* CurrentParentProcedure;
public:
	/**
	* 获取当前流程
	*/
	UFUNCTION(BlueprintPure)
	AProcedureBase* GetCurrentProcedure() const { return CurrentProcedure; }
	/**
	* 获取当前本地流程
	*/
	UFUNCTION(BlueprintPure)
	AProcedureBase* GetCurrentLocalProcedure() const { return CurrentLocalProcedure; }
	/**
	* 获取当前父流程
	*/
	UFUNCTION(BlueprintPure)
	AProcedureBase* GetCurrentParentProcedure() const { return CurrentParentProcedure; }

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
