#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Theme/WidgetStyleTypes.h"
#include "CommonWidget.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable, meta = (DisableNativeTick))
class WHFRAMEWORK_API UCommonWidget : public UCommonUserWidget, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonWidget(const FObjectInitializer& ObjectInitializer);

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintPure)
	FGameplayTag GetStyleTag() const;

	UFUNCTION(BlueprintCallable)
	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void SetInheritedStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void ClearStyleOverride();

	UFUNCTION(BlueprintCallable)
	void RefreshStyle();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;
	virtual EWidgetStyleKind GetStyleKind() const;
	virtual void BuildNativeLayout();
	virtual void ApplyResolvedStyle(const UObject* InStyle);
	virtual void BindControlEvents();
	virtual void UnbindControlEvents();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Style"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideStyleTag = false;

	UPROPERTY(Transient)
	FGameplayTag InheritedStyleTag;

	bool bSynchronizingValue = false;

private:
	void EnsureThemeSubscription();
	void ReleaseThemeSubscription();
	FDelegateHandle ThemeChangedHandle;
	bool bApplyingStyle = false;
};
