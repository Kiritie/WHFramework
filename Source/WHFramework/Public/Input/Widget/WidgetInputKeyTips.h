// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "InputCoreTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Pool/PoolWidgetBase.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif

#include "WidgetInputKeyTips.generated.h"

class UBorder;
class UHorizontalBox;
class UCommonTextBlockN;
class USizeBox;
class UCommonInputSubsystem;
class UEnhancedInputLocalPlayerSubsystem;
enum class ECommonInputType : uint8;

UCLASS(BlueprintType, meta = (DisplayName = "Input Key Tips"))
class WHFRAMEWORK_API UWidgetInputKeyTips : public UPoolWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetInputKeyTips(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void SynchronizeProperties() override;

	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	virtual void BeginDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	UFUNCTION(BlueprintCallable)
	void RefreshData();

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UHorizontalBox* Box_KeyIcon;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UHorizontalBox* HorizontalBox;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UHorizontalBox* Box_KeyCode;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	USizeBox* KeyCodeSize;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UBorder* Border_KeyCode;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UCommonTextBlockN* Txt_KeyCode;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Components")
	UCommonTextBlockN* Txt_DisplayName;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Text"))
	FGameplayTag KeyTextStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Text"))
	FGameplayTag NameTextStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Brush"))
	FGameplayTag KeyBorderStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	bool bNameBeforeKeys;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	bool bShowKeyBorder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
	FVector2D KeyIconMaxSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	FVector2D KeyCodeMinSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float KeyNameSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float NameMinWidth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TEnumAsByte<EVerticalAlignment> KeyVerticalAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TEnumAsByte<EHorizontalAlignment> KeyTextHorizontalAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
	float KeySpacing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FMargin KeyCodePadding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FText KeyDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	TArray<FGameplayTag> ActionTags;

	/** 可选的语义映射行过滤，例如同一轴动作中的上升与下潜。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FName> KeyMappingNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FKey> FixedKeys;

public:
	UFUNCTION(BlueprintPure)
	FText GetKeyDisplayName() const;

	UFUNCTION(BlueprintCallable)
	void SetKeyDisplayName(const FText InKeyDisplayName);

	UFUNCTION(BlueprintPure, Category = "Input")
	const TArray<FGameplayTag>& GetActionTags() const { return ActionTags; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetActionTags(const TArray<FGameplayTag>& InActionTags);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetKeyMappingNames(const TArray<FName>& InMappingNames);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetFixedKeys(const TArray<FKey>& InKeys);

private:
	void BuildNativeLayout();
	void EnsureThemeSubscription();
	void ReleaseThemeSubscription();
	void RefreshPresentation();
	void BindInputObservers();
	void UnbindInputObservers();
	void OnInputMethodChanged(ECommonInputType InInputType);

	TWeakObjectPtr<UCommonInputSubsystem> InputSubsystem;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;
	FDelegateHandle InputMethodChangedHandle;
	FDelegateHandle ThemeChangedHandle;

#if WITH_EDITOR
	bool RefreshDeferredInput(float InDeltaTime);
	void CancelDeferredInputRefresh();
	FTSTicker::FDelegateHandle DeferredInputRefreshHandle;
#endif
};
