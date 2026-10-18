// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonTabListWidgetBase.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Types/SlateEnums.h"

#include "CommonTabList.generated.h"

class UPanelWidget;
class UCommonScroll;

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Tab List", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonTabList : public UCommonTabListWidgetBase, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonTabList(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
	
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetButtonStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable, Category = "Layout")
	void SetOrientation(EOrientation InOrientation);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;
	virtual void HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;

	UPROPERTY(BlueprintReadOnly, Category = "Components", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabContent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Button"))
	FGameplayTag ButtonStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TEnumAsByte<EOrientation> Orientation = Orient_Horizontal;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCommonScroll> NativeScroll;
};
