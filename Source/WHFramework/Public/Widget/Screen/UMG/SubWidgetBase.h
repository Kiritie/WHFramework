// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UserWidgetBase.h"
#include "Blueprint/UserWidget.h"
#include "Common/CommonTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "SubWidgetBase.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API USubWidgetBase : public UUserWidget, public IBaseWidgetInterface, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	USubWidgetBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnCreate")
	void K2_OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize(const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnInitialize(const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(bool bRecovery = false);
	UFUNCTION()
	virtual void OnDestroy(bool bRecovery = false) override;

public:
	UFUNCTION(BlueprintCallable)
	void Refresh() override;

	UFUNCTION(BlueprintCallable)
	void Destroy(bool bRecovery = false) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UUserWidgetBase* OwnerWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn))
	TArray<FParameter> WidgetParams;

public:
	template<class T>
	T* GetOwnerWidget() const
	{
		return Cast<T>(GetOwnerWidget());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetOwnerWidget(TSubclassOf<UUserWidgetBase> InClass = nullptr) const { return GetDeterminesOutputObject(OwnerWidget, InClass); }

	UFUNCTION(BlueprintPure)
	TArray<FParameter>& GetWidgetParams() { return WidgetParams; }

	UFUNCTION(BlueprintPure)
	virtual TArray<UWidget*> GetAllPoolWidgets() const override;
};
