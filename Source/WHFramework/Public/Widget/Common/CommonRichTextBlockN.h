// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonRichTextBlock.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonRichTextBlockN.generated.h"

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Rich Text N", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonRichTextBlockN : public UCommonRichTextBlock, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonRichTextBlockN(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void SetText(const FText& InText) override;
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable)
	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void SetTextScrollStyleTag(FGameplayTag InStyleTag);

public:
	UFUNCTION(BlueprintCallable)
	void RefreshRichStyle();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	bool bUseCommonRichStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "UI.Style.TextScroll"))
	FGameplayTag TextScrollStyleTag;

private:
	virtual void BeginDestroy() override;
	void ApplyStyleTags();
	FDelegateHandle ThemeChangedHandle;
	bool bApplyingStyleTags = false;
	TSharedPtr<class FCommonTextScrollStyleBinding> TextScrollBinding;
};
