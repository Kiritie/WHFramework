#pragma once

#include "Scene/SceneModuleTypes.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "WidgetSceneMarkerPanel.generated.h"

/** 世界地图、小地图、罗盘与世界指示器共用的标记数据源 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetSceneMarkerPanel : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer);

	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;
	virtual void OnRefresh() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnMarkersUpdated")
	void K2_OnMarkersUpdated(const TArray<FSceneMarkerView>& InMarkers);

	UFUNCTION(BlueprintPure)
	TArray<FSceneMarkerView> GetMarkers() const { return Markers; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	ESceneMarkerChannel MarkerChannel = ESceneMarkerChannel::Map;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TArray<FSceneMarkerView> Markers;

	virtual FVector GetMarkerViewLocation() const;
	virtual float GetMarkerViewYaw() const;
};
