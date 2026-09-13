// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Blueprint/UserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Slate/Runtime/Interfaces/BaseWidgetInterface.h"

#include "PoolWidgetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UPoolWidgetBase : public UUserWidget, public IBaseWidgetInterface, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UPoolWidgetBase(const FObjectInitializer& ObjectInitializer);

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
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy) override;

protected:
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly)
	UUserWidget* OwnerWidget;

	UPROPERTY(Transient, BlueprintReadWrite)
	FParameter WidgetParam;

public:
	template<class T>
	T* GetOwnerWidget() const
	{
		return Cast<T>(GetOwnerWidget());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidget* GetOwnerWidget(TSubclassOf<UUserWidget> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	virtual FParameter GetWidgetParams() const { return WidgetParam; }
};
