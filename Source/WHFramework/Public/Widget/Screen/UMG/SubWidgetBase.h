// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "SubWidgetBase.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API USubWidgetBase : public UUserWidget, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	USubWidgetBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize(const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh();

	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy();

public:
	UFUNCTION(BlueprintCallable)
	void Refresh();

	UFUNCTION(BlueprintCallable)
	void Destroy(bool bRecovery = false);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UUserWidgetBase* Owner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn))
	TArray<FParameter> Params;

public:
	template<class T>
	T* GetOwner() const
	{
		return Cast<T>(GetOwner());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetOwner(TSubclassOf<UUserWidgetBase> InClass = nullptr) const { return Owner; }

	UFUNCTION(BlueprintPure)
	TArray<FParameter>& GetParams() { return Params; }
};
