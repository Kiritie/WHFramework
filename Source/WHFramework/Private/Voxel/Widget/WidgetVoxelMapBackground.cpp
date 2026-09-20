#include "Voxel/Widget/WidgetVoxelMapBackground.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/VoxelModule.h"

namespace
{
FLinearColor GetBiomeColor(const FVoxelGenerationRuntimeConfig& InConfig, uint16 InBiomeIndex)
{
	if (!InConfig.Recipe.IsValid() || !InConfig.Recipe->Biomes.IsValidIndex(InBiomeIndex))
	{
		return FLinearColor(0.22f, 0.52f, 0.16f);
	}

	const uint32 Hash = GetTypeHash(InConfig.Recipe->Biomes[InBiomeIndex].StableId);
	const uint8 Hue = static_cast<uint8>(Hash & 0xffu);
	return FLinearColor::MakeFromHSV8(Hue, 150, 180);
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
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> GenerationConfig = VoxelModule.GetGenerationConfig();
	if (!GenerationConfig)
	{
		return BaseLayer + 1;
	}

	const double BlockSize = VoxelModule.BlockSize();
	const float PixelsPerWorldUnit = PanelSize.X / MapView.Range;
	const int32 Stride = GetLODStride(static_cast<float>(BlockSize) * PixelsPerWorldUnit);
	const int32 HalfCellsX = FMath::CeilToInt(MapView.Range / BlockSize * 0.5);
	const int32 HalfCellsY = FMath::CeilToInt(MapView.Range * PanelSize.Y / PanelSize.X / BlockSize * 0.5);
	if (!FMath::IsFinite(BlockSize) || BlockSize <= 0.0)
	{
		return BaseLayer + 1;
	}
	const FIntVector CenterIndex(
		FMath::FloorToInt(MapView.Center.X / BlockSize),
		FMath::FloorToInt(MapView.Center.Y / BlockSize),
		0);

	for (int32 Y = CenterIndex.Y - HalfCellsY; Y <= CenterIndex.Y + HalfCellsY; Y += Stride)
	{
		for (int32 X = CenterIndex.X - HalfCellsX; X <= CenterIndex.X + HalfCellsX; X += Stride)
		{
			FVoxelColumnSample Column;
			FString Error;
			if (!Generator->SampleColumn(X, Y, Column, Error))
			{
				continue;
			}
			FLinearColor CellColor = GetBiomeColor(*GenerationConfig, Column.BiomeIndex) * MapTintColor;
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
