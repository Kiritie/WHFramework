// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonTextBlock.h"
#include "GameplayTagContainer.h"

#include "CommonTextBlockN.generated.h"

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI,
	   meta = (Category = "Common UI", DisplayName = "Common Text N", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonTextBlockN : public UCommonTextBlock, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonTextBlockN(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

	virtual void OnBindingChanged(const FName& Property) override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Text"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.TextScroll"))
	FGameplayTag TextScrollStyleTag;

private:
	void ApplyStyleTag();
	void EnsureThemeSubscription();
	FDelegateHandle ThemeChangedHandle;
	virtual void BeginDestroy() override;

	bool bApplyingStyleTag = false;
	TSharedPtr<class FCommonTextScrollStyleBinding> TextScrollBinding;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetHighlightText", BlueprintSetter = "SetHighlightText",
			  Category = "Content", meta = (MultiLine = "true"))
	FText HighlightText;

	UPROPERTY()
	FGetText HighlightTextDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetHighlightColor", BlueprintSetter = "SetHighlightColor",
			  Category = "Appearance")

	FSlateColor HighlightColor;

	UPROPERTY()
	FGetSlateColor HighlightColorDelegate;

	PROPERTY_BINDING_IMPLEMENTATION(FText, HighlightText);
	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, HighlightColor);

public:
	ETextWrappingPolicy GetTextWrappingPolicy() const { return WrappingPolicy; }
	ETextJustify::Type GetTextJustification() const { return Justification; }

	UFUNCTION(BlueprintPure, Category = "Widget")
	virtual FText GetHighlightText() const;

	UFUNCTION(BlueprintCallable, Category = "Widget")

	virtual void SetHighlightText(const FText InText);

	UFUNCTION(BlueprintPure, Category = "Widget")

	virtual FSlateColor GetHighlightColor() const;

	UFUNCTION(BlueprintCallable, Category = "Widget")

	virtual void SetHighlightColor(const FSlateColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Style")

	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetTextScrollStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintPure, Category = "Style")

	FGameplayTag GetStyleTag() const
	{
		return StyleTag;
	}
};
