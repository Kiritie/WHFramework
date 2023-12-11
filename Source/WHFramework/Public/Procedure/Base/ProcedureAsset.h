// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Asset/Base/AssetBase.h"
#include "ProcedureAsset.generated.h"

class UProcedureBase;
/**
 * Imported spreadsheet table.
 */
UCLASS(BlueprintType, Meta = (LoadBehavior = "LazyOnDemand"))
class WHFRAMEWORK_API UProcedureAsset : public UAssetBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Initialize(UAssetBase* InSource) override;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure Stats
public:
	/// 初始流程 
	UPROPERTY(VisibleAnywhere)
	UProcedureBase* FirstProcedure;
	/// 流程列表
	UPROPERTY(VisibleAnywhere)
	TArray<UProcedureBase*> Procedures;
	/// 流程Map
	UPROPERTY(Transient)
	TMap<TSubclassOf<UProcedureBase>, UProcedureBase*> ProcedureMap;

public:
	/**
	* 获取初始流程
	*/
	UFUNCTION(BlueprintPure)
	UProcedureBase* GetFirstProcedure() const { return FirstProcedure; }
	/**
	* 设置初始流程
	*/
	UFUNCTION(BlueprintCallable)
	void SetFirstProcedure(UProcedureBase* InFirstProcedure) { this->FirstProcedure = InFirstProcedure; }
	/**
	* 获取流程列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UProcedureBase*>& GetProcedures() { return Procedures; }
	/**
	* 获取流程Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>& GetProcedureMap() { return ProcedureMap; }

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	void GenerateProcedureListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems, const FString& InFilterText = TEXT(""));

	void UpdateProcedureListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);

	bool CanAddProcedure(TSubclassOf<UProcedureBase> InProcedureClass);

	void ClearAllProcedure();
#endif
};
