// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonUserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"

#include "SubWidgetBase.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API USubWidgetBase : public UCommonUserWidget, public ISubWidgetInterface, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	USubWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnCreate")
	void K2_OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize(const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset(bool bForce = false);
	UFUNCTION()
	virtual void OnReset(bool bForce = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(bool bRecovery);
	UFUNCTION()
	virtual void OnDestroy(bool bRecovery) override;

public:
	virtual void Init(const TArray<FParameter>* InParams) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual void Init(const TArray<FParameter>& InParams) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(bool bRecovery = false) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn))
	TArray<FParameter> WidgetParams;

	IPanelWidgetInterface* OwnerWidget;

public:
	template<class T>
	T* GetOwnerWidget() const
	{
		return Cast<T>(GetOwnerWidget());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UUserWidget* GetOwnerWidget(TSubclassOf<UUserWidget> InClass = nullptr) const override;

	UFUNCTION(BlueprintPure)
	virtual TArray<FParameter> GetWidgetParams() const override { return WidgetParams; }

	UFUNCTION(BlueprintPure)
	TArray<UWidget*> GetPoolWidgets() const;
};
