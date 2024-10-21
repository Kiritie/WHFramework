// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Blueprint/UserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "PoolWidgetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UPoolWidgetBase : public UUserWidget, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UPoolWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UUserWidget* OwnerWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn))
	TArray<FParameter> WidgetParams;

public:
	template<class T>
	T* GetOwnerWidget() const
	{
		return Cast<T>(GetOwnerWidget());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidget* GetOwnerWidget(TSubclassOf<UUserWidget> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	TArray<FParameter>& GetWidgetParams() { return WidgetParams; }
};