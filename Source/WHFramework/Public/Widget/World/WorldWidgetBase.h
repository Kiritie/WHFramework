// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetInterfaceBase.h"
#include "Widget/WidgetModuleTypes.h"

#include "WorldWidgetBase.generated.h"

class UCanvasPanelSlot;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWorldWidgetBase : public UUserWidget, public IWidgetInterfaceBase
{
	GENERATED_BODY()

public:
	UWorldWidgetBase(const FObjectInitializer& ObjectInitializer);

public:
	void TickWidget_Implementation() override;

	bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnCreate(AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Refresh();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Destroy();
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindWidgetPoint(UWidget* InWidget, class USceneComponent* InSceneComp);
		
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindWidgetPointByName(UWidget* InWidget, FName InPointName);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UnBindWidgetPoint(UWidget* InWidget);

protected:
	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

	UPROPERTY(EditDefaultsOnly)
	bool bWidgetAutoSize;
	
	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly)
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bWidgetAutoSize == false"))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly)
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;
	
	UPROPERTY(EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(Transient)
	AActor* OwnerActor;
	
	UPROPERTY(Transient)
	int32 WidgetIndex;
	
	UPROPERTY(Transient)
	class UWorldWidgetComponent* WidgetComponent;

	UPROPERTY(Transient)
	TMap<UWidget*, class USceneComponent*> BindWidgetMap;

private:
	FTimerHandle RefreshTimerHandle;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const { return WidgetName; }

	UFUNCTION(BlueprintPure)
	bool IsWidgetAutoSize() const { return bWidgetAutoSize; }
	
	UFUNCTION(BlueprintPure)
	int32 GetWidgetZOrder() const { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	FAnchors GetWidgetAnchors() const { return WidgetAnchors; }

	UFUNCTION(BlueprintPure)
	FMargin GetWidgetOffsets() const { return WidgetOffsets; }

	UFUNCTION(BlueprintPure)
	FVector2D GetWidgetAlignment() const { return WidgetAlignment; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const { return WidgetRefreshType; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const { return OwnerActor; }

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
