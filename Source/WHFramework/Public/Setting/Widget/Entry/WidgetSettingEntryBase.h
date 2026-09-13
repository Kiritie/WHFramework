#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Widget/Common/CommonButton.h"

#include "WidgetSettingEntryBase.generated.h"

class USettingEntry;

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
	virtual void NativeOnListItemObjectSet(UObject* InListItemObject) override;

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

public:
	UFUNCTION(BlueprintPure)
	USettingEntry* GetSettingEntry() const { return SettingEntry; }
};
