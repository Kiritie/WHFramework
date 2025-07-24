// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Groups/CommonButtonGroupBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonButtonGroup.generated.h"

class UCommonButton;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UCommonButtonGroup : public UCommonButtonGroupBase, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonButtonGroup();
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void OnWidgetAdded(UWidget* NewWidget) override;
	
	virtual void OnSelectionStateChangedBase(UCommonButtonBase* BaseButton, bool bIsSelected) override;

public:
	UFUNCTION(BlueprintCallable, Category = BaseButtonGroup)
	void SetSelectionRequiredN(bool bRequireSelection);

protected:
	UPROPERTY()
	bool bBroadcastOnDeselected;

public:
	UFUNCTION(BlueprintPure)
	bool IsBroadcastOnDeselected() const { return bBroadcastOnDeselected; }

	UFUNCTION(BlueprintCallable)
	void SetBroadcastOnDeselected(bool bInBroadcastOnDeselected) { bBroadcastOnDeselected = bInBroadcastOnDeselected; }

	UFUNCTION(BlueprintPure)
	TArray<UCommonButtonBase*> GetButtons()
	{
		TArray<UCommonButtonBase*> ReturnValues;
		for(auto Iter : Buttons)
		{
			ReturnValues.Add(Iter.Get());
		}
		return  ReturnValues;
	}
	
	UFUNCTION(BlueprintCallable, Category = BaseButtonGroup)
	UCommonButton* GetButtonBaseAtIndexN(int32 Index) const;
};
