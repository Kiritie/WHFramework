// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "GameplayTagContainer.h"
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
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void NativePreConstruct() override;

	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	virtual void NativeOnCurrentTextStyleChanged() override;

	virtual void NativeOnClicked() override;

	virtual void NativeOnSelected(bool bBroadcast) override;

	virtual void NativeOnDeselected(bool bBroadcast) override;
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UImage* Img_Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_Title;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"), Category = "Selection")
	bool bStandalone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn))
	FParameter WidgetParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Button"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag TriggeringActionTag;

private:
	void ApplyStyleTag();

	void ApplyTriggeringActionTag();

public:
	UFUNCTION(BlueprintPure)
	FText GetTitle() const { return Title; }

	UFUNCTION(BlueprintCallable)
	void SetTitle(const FText InTitle);

	UFUNCTION(BlueprintPure)
	bool IsStandalone() const { return bStandalone; }

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = InBrush))
	void SetIconBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable)
	void SetMinWidth(int32 InValue);

	UFUNCTION(BlueprintCallable)
	void SetMinHeight(int32 InValue);

	UFUNCTION(BlueprintPure)
	virtual FParameter GetWidgetParams() const { return WidgetParams; }

	UFUNCTION(BlueprintCallable)
	void SetIsEnabledN(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintPure, Category = "Style")
	FGameplayTag GetStyleTag() const { return StyleTag; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetTriggeringActionTag(FGameplayTag InActionTag);
};
