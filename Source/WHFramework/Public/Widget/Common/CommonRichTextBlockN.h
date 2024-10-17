// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonRichTextBlock.h"
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
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void SetText(const FText& InText) override;

public:
	UFUNCTION(BlueprintCallable)
	void RefreshRichStyle();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	bool bUseCommonRichStyle;
};
