// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Input/InputModuleTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Interfaces/BaseWidgetInterface.h"

#include "WorldWidgetBase.generated.h"

class UCanvasPanelSlot;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWorldWidgetBase : public UUserWidget, public IBaseWidgetInterface, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWorldWidgetBase(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	virtual int32 GetLimit_Implementation() const override { return 0; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnCreate(AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy(bool bRecovery = false);

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Refresh();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RefreshVisibility();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Destroy(bool bRecovery = false);

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindWidgetPoint(UWidget* InWidget, FVector InLocation, USceneComponent* InSceneComp = nullptr);
		
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindWidgetPointByName(UWidget* InWidget, FVector InLocation, FName InPointName);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UnBindWidgetPoint(UWidget* InWidget);
	
	UFUNCTION(BlueprintPure, BlueprintNativeEvent)
	FWorldWidgetBindInfo GetWidgetBindInfo(UWidget* InWidget);

protected:
	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;
	
	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly)
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly)
	bool bWidgetAutoSize;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "bWidgetAutoSize == false"))
	FVector2D WidgetDrawSize;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "bWidgetAutoSize == false"))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly)
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;
	
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetAutoVisibility;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "bWidgetAutoVisibility == true"))
	float WidgetShowDistance;

	UPROPERTY(EditDefaultsOnly)
	TArray<FParameter> WidgetParams;

	UPROPERTY(EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(Transient)
	AActor* OwnerActor;
	
	UPROPERTY(Transient)
	int32 WidgetIndex;
	
	UPROPERTY(Transient)
	class UWorldWidgetComponent* WidgetComponent;

	UPROPERTY(Transient)
	TMap<UWidget*, FWorldWidgetBindInfo> BindWidgetMap;

private:
	FTimerHandle RefreshTimerHandle;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const override { return WidgetName; }
	
	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override { return WidgetDrawSize; }

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }
	
	UFUNCTION(BlueprintPure)
	TArray<FParameter> GetWidgetParams() const { return WidgetParams; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	template<class T>
	T* GetOwnerActor() const
	{
		return Cast<T>(OwnerActor);
	}

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetIndex() const { return WidgetIndex; }

	UFUNCTION(BlueprintCallable)
	virtual void SetWidgetIndex(int32 InWidgetIndex) { WidgetIndex = InWidgetIndex; }

	UFUNCTION(BlueprintPure)
	UWorldWidgetComponent* GetWidgetComponent() const { return WidgetComponent; }

	UFUNCTION(BlueprintPure)
	EWidgetSpace GetWidgetSpace() const;
	
	UFUNCTION(BlueprintPure)
	virtual class UPanelWidget* GetRootPanelWidget() const override;
};
