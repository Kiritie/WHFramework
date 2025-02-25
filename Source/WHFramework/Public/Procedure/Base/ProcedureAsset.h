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
	GENERATED_BODY()

public:
	UProcedureAsset(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void Initialize() override;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure Stats
public:
	/// 自动切换初始流程 
	UPROPERTY(EditAnywhere)
	bool bAutoSwitchFirst;
	/// 初始流程 
	UPROPERTY(VisibleAnywhere)
	UProcedureBase* FirstProcedure;
	/// 流程列表
	UPROPERTY(VisibleAnywhere)
	TArray<UProcedureBase*> Procedures;
	/// 流程Map
	UPROPERTY(Transient)
	TMap<TSubclassOf<UProcedureBase>, UProcedureBase*> ProcedureMap;

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
