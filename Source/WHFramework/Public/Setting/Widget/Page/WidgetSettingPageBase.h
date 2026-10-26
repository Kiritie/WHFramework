#pragma once

#include "Widget/Sub/SubActivatableWidgetBase.h"
#include "Widget/Style/CommonStyleTypes.h"

#include "WidgetSettingPageBase.generated.h"

class UCommonListView;
class USettingEntry;
class USettingCategoryEntry;
class UWidgetSettingEntryBase;
class UWidget;
class USettingModule;
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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
	void SetSettingSource(USettingModule* InSource);

	UFUNCTION(BlueprintCallable)
	void RefreshEntries();

protected:
	TSubclassOf<UUserWidget> ResolveEntryClass(UObject* InItem) const;

private:
	USettingModule* GetSettingSource() const;
	TWeakObjectPtr<USettingModule> SettingSource;
	void HandleThemeChanged();
	void HandleEntryWidgetGenerated(UUserWidget& InEntryWidget);
	TWeakObjectPtr<UObject> PendingFocusItem;
	FName PendingFocusWidgetName;
	FDelegateHandle ThemeChangedHandle;

	UPROPERTY(Transient)
	FGameplayTag PageLabelTextStyleTag;

	UPROPERTY(Transient)
	TMap<FName, FGameplayTag> PageRendererLabelTextStyleTags;

	UPROPERTY(Transient)
	FCommonControlStyleTags PageControlStyles;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget), Category = "Components")
	TObjectPtr<UCommonListView> SettingList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> EmptyState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USettingCategoryEntry>> CategoryEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FName PageName;

public:
	FText GetTitle() const { return Title; }
};
