// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonButton.generated.h"

class UImage;
class UCommonTextBlockN;
class UCommonButtonStyleN;
class UStackBox;
class USizeBox;

UCLASS(BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Button", DisableNativeTick))
class WHFRAMEWORK_API UCommonButton : public UCommonButtonBase, public IObjectPoolInterface
{
	friend class UCommonButtonGroup;
	friend class FCommonIconButtonStateTest;

	GENERATED_BODY()

public:
	UCommonButton(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

public:
	virtual void SynchronizeProperties() override;

	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnCurrentTextStyleChanged() override;

	virtual void NativeOnClicked() override;

	virtual void NativeOnPressed() override;

	virtual void NativeOnReleased() override;

	virtual void NativeOnSelected(bool bBroadcast) override;

	virtual void NativeOnDeselected(bool bBroadcast) override;

protected:
	virtual UCommonButtonInternalBase* ConstructInternalButton() override;
	virtual void BuildNativeLayout();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Components")
	UImage* Img_Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_Title;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Components")
	TObjectPtr<UImage> Img_SelectionMarker;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TEnumAsByte<EOrientation> ContentOrientation = Orient_Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TEnumAsByte<EHorizontalAlignment> ContentHorizontalAlignment = HAlign_Center;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "0"))
	float ContentSpacing = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "0.0"))
	float TitleMinWidth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "0.0"))
	FVector2D ContentMinSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Brush"))
	FGameplayTag IconStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Text"))
	FGameplayTag TitleTextStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"), Category = "Selection")
	bool bStandalone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn))
	FParameter WidgetParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Button", ToolTip = "填写时使用当前 Tag；留空时使用外层传入的 Tag，外层未提供时使用类型默认样式。"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag TriggeringActionTag;

protected:
	virtual void OnResolvedStyleChanged(const UCommonButtonStyleN* InStyle);

private:
	void ApplyStyleTag();
	void EnsureThemeSubscription();
	FDelegateHandle ThemeChangedHandle;
	virtual void BeginDestroy() override;
	FGameplayTag InheritedStyleTag;
	UPROPERTY(Transient)
	TObjectPtr<UStackBox> NativeContent;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> NativeContentSize;
	UPROPERTY(Transient)
	FSlateBrush IconOverride;

	bool bHasIconOverride = false;

	void ApplyTitleStyle();
	void ApplyIconStyle(const UCommonButtonStyleN* InStyle);
	void ApplyIconPresentation();
	void ApplySelectionMarker();
	void ApplyContentLayout();

	void ApplyTriggeringActionTag();

public:
	UFUNCTION(BlueprintPure)
	FText GetTitle() const
	{
		return Title;
	}

	UFUNCTION(BlueprintCallable)

	void SetTitle(const FText InTitle);

	UFUNCTION(BlueprintCallable)
	void SetTriggerClickedAfterSelection(bool bInTriggerClickedAfterSelection);

	UFUNCTION(BlueprintPure)

	bool IsStandalone() const
	{
		return bStandalone;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = InBrush))

	void SetIconBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetIconStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetTitleTextStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable, Category = "Content")
	void SetContentOrientation(EOrientation InOrientation);

	UFUNCTION(BlueprintCallable, Category = "Content")
	void SetContentSpacing(float InSpacing);

	UFUNCTION(BlueprintCallable, Category = "Content")
	void SetTitleMinWidth(float InWidth);

	UFUNCTION(BlueprintCallable, Category = "Content")
	void SetContentMinSize(const FVector2D& InSize);

	UFUNCTION(BlueprintCallable)

	void SetMinWidth(int32 InValue);

	UFUNCTION(BlueprintCallable)

	void SetMinHeight(int32 InValue);

	UFUNCTION(BlueprintPure)

	virtual FParameter GetWidgetParams() const
	{
		return WidgetParams;
	}

	UFUNCTION(BlueprintCallable)

	void SetIsEnabledN(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Style")

	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetInheritedStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintPure, Category = "Style")

	FGameplayTag GetStyleTag() const;

	UFUNCTION(BlueprintCallable, Category = "Input")

	void SetTriggeringActionTag(FGameplayTag InActionTag);
};
