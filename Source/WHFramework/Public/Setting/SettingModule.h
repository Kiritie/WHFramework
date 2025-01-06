// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "SettingModule.generated.h"

class UWidgetSettingItemCategoryBase;
class UWidgetKeySettingItemBase;
class UWidgetTextSettingItemBase;
class UWidgetEnumSettingItemBase;
class UWidgetBoolSettingItemBase;
class UWidgetFloatSettingItemBase;

UCLASS()
class WHFRAMEWORK_API USettingModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(USettingModule)

public:	
	// ParamSets default values for this actor's properties
	USettingModule();

	~USettingModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetSettingItemCategoryBase> SettingItemCategoryClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetFloatSettingItemBase> FloatSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetBoolSettingItemBase> BoolSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetEnumSettingItemBase> EnumSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetTextSettingItemBase> TextSettingItemClass;

	UPROPERTY(EditAnywhere, Category = "WdigetClass")
	TSubclassOf<UWidgetKeySettingItemBase> KeySettingItemClass;

public:
	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetSettingItemCategoryBase> GetSettingItemCategoryClass() const { return SettingItemCategoryClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetFloatSettingItemBase> GetFloatSettingItemClass() const { return FloatSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetBoolSettingItemBase> GetBoolSettingItemClass() const { return BoolSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetEnumSettingItemBase> GetEnumSettingItemClass() const { return EnumSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetTextSettingItemBase> GetTextSettingItemClass() const { return TextSettingItemClass; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetKeySettingItemBase> GetKeySettingItemClass() const { return KeySettingItemClass; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
