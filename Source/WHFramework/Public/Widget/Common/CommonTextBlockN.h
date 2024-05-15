// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonTextBlock.h"

#include "CommonTextBlockN.generated.h"

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Text N", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonTextBlockN : public UCommonTextBlock, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonTextBlockN(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void SynchronizeProperties() override;

	virtual void OnBindingChanged(const FName& Property) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetHighlightText", BlueprintSetter = "SetHighlightText", Category = "Content", meta = (MultiLine = "true"))
	FText HighlightText;
	
	UPROPERTY()
	FGetText HighlightTextDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetHighlightColor", BlueprintSetter = "SetHighlightColor", Category = "Appearance")
	FSlateColor HighlightColor;

	UPROPERTY()
	FGetSlateColor HighlightColorDelegate;

	PROPERTY_BINDING_IMPLEMENTATION(FText, HighlightText);
	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, HighlightColor);

public:
	UFUNCTION(BlueprintPure, Category = "Widget")
	virtual FText GetHighlightText() const;
	
	UFUNCTION(BlueprintCallable, Category = "Widget")
	virtual void SetHighlightText(const FText InText);

	UFUNCTION(BlueprintPure, Category = "Widget")
	virtual FSlateColor GetHighlightColor() const;
	
	UFUNCTION(BlueprintCallable, Category = "Widget")
	virtual void SetHighlightColor(const FSlateColor InColor);
};
