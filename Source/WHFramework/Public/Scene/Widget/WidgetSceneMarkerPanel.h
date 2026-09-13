#pragma once

#include "Scene/SceneModuleTypes.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "WidgetSceneMarkerPanel.generated.h"

class UCanvasPanel;
class UWidgetSceneMapBase;
class UWidgetSceneMarkerItem;

/** 世界地图、小地图与罗盘共用的标记面板 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetSceneMarkerPanel : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer);

	virtual void OnInitialize(const FParameter& InParams) override;
	virtual void OnRefresh() override;
	virtual void OnDestroy(EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnMarkersUpdated")
	void K2_OnMarkersUpdated(const TArray<FSceneMarkerView>& InMarkers);

	UFUNCTION(BlueprintPure)
	TArray<FSceneMarkerView> GetMarkers() const { return Markers; }

	ESceneMarkerChannel GetMarkerChannel() const { return MarkerChannel; }
	UCanvasPanel* GetMarkerCanvas() const { return MarkerCanvas; }
	FName GetMarkerCanvasName() const { return MarkerCanvasName; }
	FName GetMarkerCanvasHostName() const { return MarkerCanvasHostName; }
	TSubclassOf<UWidgetSceneMarkerItem> GetMarkerItemClass() const { return MarkerItemClass; }
	int32 GetMarkerItemCount() const { return MarkerItems.Num(); }
	bool IsShowingMarkerNames() const { return bShowMarkerNames; }
	bool IsShowingMarkerDistance() const { return bShowMarkerDistance; }
	bool IsShowingPlayerMarker() const { return bShowPlayerMarker; }
	const FSceneMapView& GetMapView() const { return MapView; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	ESceneMarkerChannel MarkerChannel = ESceneMarkerChannel::Map;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TArray<FSceneMarkerView> Markers;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Marker")
	UCanvasPanel* MarkerCanvas;

	/** 蓝图内专用于标记投影的 Canvas；不允许按尺寸猜测其他 Canvas。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	FName MarkerCanvasName = NAME_None;

	/** 可选宿主 Overlay/Canvas；指定的 MarkerCanvas 不存在时在该宿主内创建。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	FName MarkerCanvasHostName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	TSubclassOf<UWidgetSceneMarkerItem> MarkerItemClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	bool bShowMarkerNames = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	bool bShowMarkerDistance = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	bool bShowPlayerMarker = false;

	UPROPERTY(Transient)
	TMap<FGuid, UWidgetSceneMarkerItem*> MarkerItems;

	UPROPERTY(Transient)
	UWidgetSceneMapBase* MapBackground = nullptr;

	FSceneMapView MapView;
	bool bPlayerMarkerPresent = false;

	virtual FVector GetMarkerViewLocation() const;
	virtual float GetMarkerViewYaw() const;
	virtual bool ProjectMarker(const FSceneMarkerView& InMarker, FVector2D InPanelSize, FVector2D& OutPosition) const;

	UFUNCTION()
	void RefreshMarkerMembership();

	void ResolveMarkerCanvas();
	void ResolveMapBackground();
	void AddPlayerMarker();
	void RebuildMarkerItems();
	void UpdateMarkerStates();
	void DestroyMarkerItems();
};
