#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Widget/Common/CommonButton.h"

#include "WidgetSettingEntryBase.generated.h"

class USettingEntry;
class UCommonTextBlockN;

UCLASS(Abstract)
class WHFRAMEWORK_API UWidgetSettingEntryBase : public UCommonButton, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UWidgetSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* InListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void OnResolvedStyleChanged(const USubWidgetStyle* InStyle) override;
	virtual UWidget* GetValueControl() const;
	virtual FName GetRendererName() const;
	virtual void BindValueControl();
	virtual void UnbindValueControl();
	virtual void ConfigureValueControl();
	virtual void OnModelStateChanged();

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetSettingEntry(USettingEntry* InEntry);

	UFUNCTION(BlueprintCallable)
	virtual void RefreshFromModel();

protected:
	UFUNCTION(BlueprintNativeEvent)
	void ApplyValueToControl(const FParameter& InValue);

	bool CommitUserValue(const FParameter& InValue);

	UPROPERTY(BlueprintReadOnly, Transient)
	TObjectPtr<USettingEntry> SettingEntry;

	bool bRefreshingFromModel = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlockN> Txt_Reason;

public:
	UFUNCTION(BlueprintPure)
	USettingEntry* GetSettingEntry() const { return SettingEntry; }
};
