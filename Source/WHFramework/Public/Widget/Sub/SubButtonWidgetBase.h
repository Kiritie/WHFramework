// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/Common/CommonButton.h"

#include "SubButtonWidgetBase.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API USubButtonWidgetBase : public UCommonButton, public ISubWidgetInterface
{
	GENERATED_BODY()
	
public:
	USubButtonWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnCreate")
	void K2_OnCreate(const FParameter& InParam);
	UFUNCTION()
	virtual void OnCreate(const FParameter& InParam) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize(const FParameter& InParam);
	UFUNCTION()
	virtual void OnInitialize(const FParameter& InParam) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset(bool bForce = false);
	UFUNCTION()
	virtual void OnReset(bool bForce = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(EObjectDespawnMode InMode);
	UFUNCTION()
	virtual void OnDestroy(EObjectDespawnMode InMode) override;

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	virtual void Init(const FParameter& InParam = FParameter()) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy) override;

protected:
	IPanelWidgetInterface* OwnerWidget;

	UPROPERTY(Transient)
	bool bDynamicSubWidget;

public:
	template<class T>
	T* GetOwnerWidget() const
	{
		return Cast<T>(GetOwnerWidget());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UUserWidget* GetOwnerWidget(TSubclassOf<UUserWidget> InClass = nullptr) const override;

	virtual FParameter GetWidgetParams() const override { return Super::GetWidgetParams(); }

	UFUNCTION(BlueprintPure)
	TArray<UWidget*> GetPoolWidgets() const;
};
