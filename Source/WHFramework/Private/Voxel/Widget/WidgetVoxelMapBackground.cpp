#include "Voxel/Widget/WidgetVoxelMapBackground.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/VoxelModule.h"

int32 UWidgetVoxelMapBackground::GetLODStride(const float InCellPixelSize)
{
	return FVoxelMapTileCache::SelectStep(InCellPixelSize);
}

void UWidgetVoxelMapBackground::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UVoxelModule* Module = UVoxelModule::Find(GetWorld());
	if (!Module || !Module->IsReady())
	{
		return;
	}
	FVoxelMapTileCache* Cache = Module->GetMapTileCache();
	if (!Cache)
	{
		return;
	}
	TArray<FVoxelMapTileKey> Visible;
	FVoxelMapTileCache::VisibleTiles(
		MapView, MyGeometry.GetLocalSize(), Module->BlockSize(), Visible);
	Cache->Request(Visible, MapView);
	if (LastTileRevision != Cache->GetRevision())
	{
		LastTileRevision = Cache->GetRevision();
		InvalidateLayoutAndVolatility();
	}
}

int32 UWidgetVoxelMapBackground::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);
	FSlateDrawElement::MakeBox(
		OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, BackgroundColor);

	UVoxelModule* Module = UVoxelModule::Find(GetWorld());
	if (!Module || !Module->IsReady() || !Module->GetMapTileCache() ||
		MapView.Range <= UE_SMALL_NUMBER)
	{
		return BaseLayer + 1;
	}
	const FVector2D PanelSize = AllottedGeometry.GetLocalSize();
	TArray<FVoxelMapTileKey> Visible;
	FVoxelMapTileCache::VisibleTiles(MapView, PanelSize, Module->BlockSize(), Visible);
	const float PixelsPerWorldUnit = PanelSize.X / MapView.Range;
	for (const FVoxelMapTileKey& Key : Visible)
	{
		const FSlateBrush* Brush = Module->GetMapTileCache()->FindBrush(Key);
		if (!Brush)
		{
			continue;
		}
		const double TileWorldSize = static_cast<double>(
			FVoxelMapTileCache::TileSide) * Key.Step * Module->BlockSize();
		const FVector2D WorldCenter =
			(FVector2D(Key.Coordinate.X, Key.Coordinate.Y) + FVector2D(0.5)) * TileWorldSize;
		const FVector2D Delta = (WorldCenter - MapView.Center).GetRotated(-MapView.Yaw);
		const FVector2D DrawSize(TileWorldSize * PixelsPerWorldUnit);
		const FVector2D DrawPosition = PanelSize * 0.5 +
			FVector2D(Delta.X, -Delta.Y) * PixelsPerWorldUnit - DrawSize * 0.5;
		FSlateDrawElement::MakeRotatedBox(
			OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(DrawPosition))),
			Brush, ESlateDrawEffect::None, FMath::DegreesToRadians(MapView.Yaw),
			TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement,
			MapTintColor);
	}
	return BaseLayer + 2;
}
