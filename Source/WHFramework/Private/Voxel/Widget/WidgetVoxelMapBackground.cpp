#include "Voxel/Widget/WidgetVoxelMapBackground.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

namespace
{
FLinearColor GetBiomeColor(EVoxelBiomeId InBiome)
{
	switch (InBiome)
	{
		case EVoxelBiomeId::Forest:
			return FLinearColor(0.08f, 0.32f, 0.08f);
		case EVoxelBiomeId::Desert:
			return FLinearColor(0.72f, 0.60f, 0.28f);
		case EVoxelBiomeId::Snow:
			return FLinearColor(0.82f, 0.88f, 0.92f);
		case EVoxelBiomeId::Mountain:
			return FLinearColor(0.34f, 0.34f, 0.36f);
		case EVoxelBiomeId::Ocean:
			return FLinearColor(0.05f, 0.22f, 0.56f);
		case EVoxelBiomeId::Plains:
		default:
			return FLinearColor(0.22f, 0.52f, 0.16f);
	}
}
}

int32 UWidgetVoxelMapBackground::GetLODStride(float InCellPixelSize)
{
	int32 Stride = 1;
	while (Stride < 8 && InCellPixelSize * Stride < 8.f)
	{
		Stride *= 2;
	}
	return Stride;
}

int32 UWidgetVoxelMapBackground::NativePaint(const FPaintArgs& Args,
                                             const FGeometry& AllottedGeometry,
                                             const FSlateRect& MyCullingRect,
                                             FSlateWindowElementList& OutDrawElements,
                                             int32 LayerId,
                                             const FWidgetStyle& InWidgetStyle,
                                             bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D PanelSize = AllottedGeometry.GetLocalSize();
	FSlateDrawElement::MakeBox(OutDrawElements,
	                           BaseLayer + 1,
	                           AllottedGeometry.ToPaintGeometry(),
	                           FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
	                           ESlateDrawEffect::None,
	                           BackgroundColor);

	UVoxelModule& VoxelModule = UVoxelModule::Get();
	if (!VoxelModule.IsReady() || MapView.Range <= UE_SMALL_NUMBER || PanelSize.IsNearlyZero())
	{
		return BaseLayer + 1;
	}
	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = VoxelModule.GetGenerator();
	if (!Generator)
	{
		return BaseLayer + 1;
	}

	const double BlockSize = VoxelModule.BlockSize();
	const float PixelsPerWorldUnit = PanelSize.X / MapView.Range;
	const int32 Stride = GetLODStride(static_cast<float>(BlockSize) * PixelsPerWorldUnit);
	const int32 HalfCellsX = FMath::CeilToInt(MapView.Range / BlockSize * 0.5);
	const int32 HalfCellsY = FMath::CeilToInt(MapView.Range * PanelSize.Y / PanelSize.X / BlockSize * 0.5);
	FIntVector CenterIndex;
	if (!VoxelCoord::FromWorld(FVector(MapView.Center, 0.0), BlockSize, CenterIndex))
	{
		return BaseLayer + 1;
	}

	for (int32 Y = CenterIndex.Y - HalfCellsY; Y <= CenterIndex.Y + HalfCellsY; Y += Stride)
	{
		for (int32 X = CenterIndex.X - HalfCellsX; X <= CenterIndex.X + HalfCellsX; X += Stride)
		{
			const FVoxelColumnSample Column = Generator->SampleColumn(X, Y);
			FLinearColor CellColor = GetBiomeColor(Column.Biome) * MapTintColor;
			FVoxelBlockState State;
			for (int32 Z = VoxelModule.GetManifest().Settings.MaxZ - 1; Z >= VoxelModule.GetManifest().Settings.MinZ; --Z)
			{
				if (!VoxelModule.GetRuntime()->TryGetBlock(FIntVector(X, Y, Z), State))
				{
					break;
				}
				if (State.IsAir())
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Definition = VoxelModule.GetRegistry()->Find(State.TypeId);
				if (Definition && Definition->RenderGroup == EVoxelRenderGroup::Water)
				{
					CellColor = FLinearColor(0.04f, 0.26f, 0.72f) * MapTintColor;
				}
				break;
			}

			const FVector2D WorldCenter((X + Stride * 0.5) * BlockSize, (Y + Stride * 0.5) * BlockSize);
			const FVector2D Delta = (WorldCenter - MapView.Center).GetRotated(-MapView.Yaw);
			const FVector2D DrawSize(Stride * BlockSize * PixelsPerWorldUnit);
			const FVector2D DrawPosition = PanelSize * 0.5 + FVector2D(Delta.X, -Delta.Y) * PixelsPerWorldUnit - DrawSize * 0.5;
			FSlateDrawElement::MakeBox(OutDrawElements,
			                           BaseLayer + 2,
			                           AllottedGeometry.ToPaintGeometry(FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(DrawPosition))),
			                           FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
			                           ESlateDrawEffect::None,
			                           CellColor);
		}
	}
	return BaseLayer + 2;
}
