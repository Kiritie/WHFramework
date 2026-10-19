#pragma once

#include "Scene/SceneModuleTypes.h"
#include "Widget/Sub/SubWidgetBase.h"
#include "WidgetSceneMarkerItem.generated.h"

class UImage;
class UTextBlock;
class USizeBox;

/** 场景标记显示项 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetSceneMarkerItem : public USubWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetSceneMarkerItem(const FObjectInitializer& ObjectInitializer);
	static constexpr float IconSize = 24.f;

	void SetMarkerView(const FSceneMarkerView& InMarkerView, bool bInShowName, bool bInShowDistance, FGameplayTag InIconStyleTag = FGameplayTag());
	void UpdateMarkerState(const FSceneMarkerView& InMarkerView);
	void SetPlayerRotation(float InAngle);

	FGuid GetMarkerID() const { return MarkerView.Marker.MarkerID; }
	FVector2D GetIconExtent() const { return IconExtent; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scene Marker|Style")
	TSoftObjectPtr<UTexture2D> PlayerIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ImgIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TxtSymbol;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TxtBadge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TxtName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TxtDistance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSceneMarkerView MarkerView;

	bool bShowName = false;
	bool bShowDistance = false;

private:
	void RefreshIcon();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> IconSizeBox;

	FGameplayTag IconStyleTag;
	FDelegateHandle ThemeChangedHandle;
	FVector2D IconExtent = FVector2D(IconSize);
};
