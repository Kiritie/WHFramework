#pragma once

#include "Widget/Sub/SubActivatableWidgetBase.h"

#include "WidgetSettingPageBase.generated.h"

class UCommonListView;
class USettingEntry;
class UWidgetSettingEntryBase;
struct FSettingPageDefinition;

UCLASS()
class WHFRAMEWORK_API UWidgetSettingPageBase : public USubActivatableWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// SubWidgetBase
public:
	virtual void OnCreate(const FParameter& InParam) override;

	virtual void OnReset(bool bForce) override;

	virtual void OnDestroy(EObjectDespawnMode InMode) override;

	//////////////////////////////////////////////////////////////////////////
	/// CommonActivatableWidget
public:
	virtual void NativeOnActivated() override;

	virtual void NativeOnDeactivated() override;

	//////////////////////////////////////////////////////////////////////////
	/// Setting
protected:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnApply")
	void K2_OnApply();

	UFUNCTION()
	virtual void OnApply();

	UFUNCTION()
	void OnSettingValueChanged(FSettingId InSettingId);

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CanApply() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CanReset() const;

	UFUNCTION(BlueprintCallable)
	void Apply();

	UFUNCTION(BlueprintCallable)
	void SetPage(FName InPage);

	void SetPageDefinition(const FSettingPageDefinition& InDefinition);

	UFUNCTION(BlueprintCallable)
	void RefreshEntries();

protected:
	TSubclassOf<UUserWidget> ResolveEntryClass(UObject* InItem) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UCommonListView> SettingList;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FName PageName;

public:
	FText GetTitle() const { return Title; }
};
