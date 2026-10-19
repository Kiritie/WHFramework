#pragma once

#include "Components/ContentWidget.h"
#include "CommonRoundedClip.generated.h"

class SCommonRoundedClip;

/** 对单个内容树应用圆角裁剪，不改变内容坐标。 */
UCLASS()
class WHFRAMEWORK_API UCommonRoundedClip : public UContentWidget
{
	GENERATED_BODY()

public:
	UCommonRoundedClip(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable)
	void SetCornerRadius(float InRadius);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float CornerRadius = 0.f;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

private:
	TSharedPtr<SCommonRoundedClip> RoundedClip;
};
