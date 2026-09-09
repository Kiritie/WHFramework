#include "Voxel/Widget/WidgetVoxelMapBackground.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Voxel/VoxelModule.h"

int32 UWidgetVoxelMapBackground::GetLODStride(float InCellPixelSize)
{
	int32 Stride = 1;
	while(Stride < 8 && InCellPixelSize * Stride < 8.f) Stride *= 2;
	return Stride;
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
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D PanelSize(AllottedGeometry.GetLocalSize());
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None,
		FLinearColor::White);
	if(!UVoxelModule::IsValid() || MapView.Range <= UE_SMALL_NUMBER || PanelSize.IsNearlyZero()) return BaseLayer + 1;

	const float PixelsPerWorldUnit = PanelSize.X / MapView.Range;
	const FVector2D ViewHalfWorld(MapView.Range * 0.5f, MapView.Range * PanelSize.Y / PanelSize.X * 0.5f);
	const float Rotation = FMath::DegreesToRadians(MapView.Yaw);
	const float AbsCos = FMath::Abs(FMath::Cos(Rotation));
	const float AbsSin = FMath::Abs(FMath::Sin(Rotation));
	UVoxelModule::Get().ForEachChunk([&](const UVoxelChunk& VoxelChunk)
	{
		VoxelChunk.ReadVoxelMap([&](const FVoxelMapChunk& Chunk)
		{
			if(Chunk.CellSize <= UE_SMALL_NUMBER || Chunk.Size.X <= 0 || Chunk.Size.Y <= 0 ||
				Chunk.Cells.Num() != Chunk.Size.X * Chunk.Size.Y) return;

			const FVector2D ChunkWorldSize = FVector2D(Chunk.Size) * Chunk.CellSize;
			const FVector2D ChunkHalfWorld = ChunkWorldSize * 0.5f;
			const FVector2D RotatedChunkHalfWorld(
				AbsCos * ChunkHalfWorld.X + AbsSin * ChunkHalfWorld.Y,
				AbsSin * ChunkHalfWorld.X + AbsCos * ChunkHalfWorld.Y);
			const FVector2D ChunkDelta = (Chunk.Origin + ChunkHalfWorld - MapView.Center).GetRotated(-MapView.Yaw);
			if(FMath::Abs(ChunkDelta.X) > ViewHalfWorld.X + RotatedChunkHalfWorld.X ||
				FMath::Abs(ChunkDelta.Y) > ViewHalfWorld.Y + RotatedChunkHalfWorld.Y) return;

			const int32 Stride = GetLODStride(Chunk.CellSize * PixelsPerWorldUnit);
			for(int32 Y = 0; Y < Chunk.Size.Y; Y += Stride)
			{
				const int32 GroupHeight = FMath::Min(Stride, Chunk.Size.Y - Y);
				for(int32 X = 0; X < Chunk.Size.X; X += Stride)
				{
					const int32 GroupWidth = FMath::Min(Stride, Chunk.Size.X - X);
					const FVoxelMapCell* Cell = nullptr;
					for(int32 CellY = Y; CellY < Y + GroupHeight; ++CellY)
					{
						for(int32 CellX = X; CellX < X + GroupWidth; ++CellX)
						{
							const FVoxelMapCell& Candidate = Chunk.Cells[CellY * Chunk.Size.X + CellX];
							if(Candidate.Texture && (!Cell || Candidate.Height > Cell->Height)) Cell = &Candidate;
						}
					}
					if(!Cell) continue;

					const FVector2D GroupWorldSize(GroupWidth * Chunk.CellSize, GroupHeight * Chunk.CellSize);
					const FVector2D WorldCenter = Chunk.Origin + FVector2D(
						(X + GroupWidth * 0.5f) * Chunk.CellSize,
						(Y + GroupHeight * 0.5f) * Chunk.CellSize);
					const FVector2D Delta = (WorldCenter - MapView.Center).GetRotated(-MapView.Yaw);
					if(FMath::Abs(Delta.X) > ViewHalfWorld.X + GroupWorldSize.GetMax() ||
						FMath::Abs(Delta.Y) > ViewHalfWorld.Y + GroupWorldSize.GetMax()) continue;

					const FVector2D DrawSize = GroupWorldSize * PixelsPerWorldUnit;
					const FVector2D DrawCenter = PanelSize * 0.5f + FVector2D(Delta.X, -Delta.Y) * PixelsPerWorldUnit;
					const FVector2D DrawPosition = DrawCenter - DrawSize * 0.5f;
					FSlateBrush CellBrush;
					CellBrush.SetResourceObject(Cell->Texture.Get());
					CellBrush.ImageSize = FVector2f(DrawSize);
					CellBrush.SetUVRegion(FBox2f(FVector2f(Cell->UVCorner), FVector2f(Cell->UVCorner + Cell->UVSpan)));
					FSlateDrawElement::MakeRotatedBox(
						OutDrawElements,
						BaseLayer + 2,
						AllottedGeometry.ToPaintGeometry(FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(DrawPosition))),
						&CellBrush,
						ESlateDrawEffect::None,
						Rotation + FMath::DegreesToRadians(static_cast<uint8>(Cell->Angle) * 90.f),
						FVector2f(DrawSize * 0.5f),
						FSlateDrawElement::RelativeToElement,
						FLinearColor::White);
				}
			}
		});
	});
	return BaseLayer + 2;
}
