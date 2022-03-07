// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"

#include "WorldWidgetBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWorldWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UWorldWidgetBase(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName WidgetName;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	int32 WidgetZOrder;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FAnchors WidgetAnchors;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bWidgetAutoSize;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "bWidgetAutoSize == false"))
	FMargin WidgetOffsets;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FVector2D WidgetAlignment;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(BlueprintReadOnly)
	AActor* OwnerActor;
	
	UPROPERTY(BlueprintReadOnly)
	int32 WidgetIndex;
	
	UPROPERTY(BlueprintReadOnly)
	class UWorldWidgetComponent* WidgetComponent;

	UPROPERTY()
	TMap<UWidget*, class USceneComponent*> BindWidgetMap;

private:
	FTimerHandle RefreshTimerHandle;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const { return WidgetName; }

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
	virtual class UPanelWidget* GetRootPanelWidget() const;
};
