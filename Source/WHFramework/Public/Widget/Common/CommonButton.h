// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonButton.generated.h"

class UImage;
class UCommonTextBlockN;

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Button", DisableNativeTick))
class WHFRAMEWORK_API UCommonButton : public UCommonButtonBase, public IObjectPoolInterface
{
	friend class UCommonButtonGroup;

	GENERATED_BODY()

public:
	UCommonButton(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void NativePreConstruct() override;

	virtual void NativeOnCurrentTextStyleChanged() override;

	virtual void NativeOnClicked() override;

	virtual void NativeOnSelected(bool bBroadcast) override;

	virtual void NativeOnDeselected(bool bBroadcast) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UImage* Img_Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonTextBlockN* Txt_Title;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"), Category = "Selection")
	bool bStandalone;

	UPROPERTY(BlueprintReadOnly)
	bool bClicked;

public:
	UFUNCTION(BlueprintPure)
	FText GetTitle() const { return Title; }

	UFUNCTION(BlueprintCallable)
	void SetTitle(const FText InTitle);

	UFUNCTION(BlueprintPure)
	bool IsStandalone() const { return bStandalone; }

	UFUNCTION(BlueprintPure)
	bool IsClicked() const { return bClicked; }

	UFUNCTION(BlueprintCallable)
	void SetIconBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable)
	void SetMinWidth(int32 InValue);

	UFUNCTION(BlueprintCallable)
	void SetMinHeight(int32 InValue);
};
