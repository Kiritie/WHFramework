#pragma once

#include "CoreMinimal.h"

// 地表网格及 Fine 交界共用同一湿区多边形和扇形三角化，保持实际线性水边缘。
namespace VoxelHeightfieldWater
{
	template<typename Visitor>
	void VisitCellPolygons(const int32 Side, const int32 Step, TConstArrayView<int32> Ground,
		TConstArrayView<int32> Water, TConstArrayView<uint8> Kind, const int32 X, const int32 Y, Visitor&& Visit)
	{
		const int32 Indices[4] = {X + Y * Side, X + 1 + Y * Side, X + 1 + (Y + 1) * Side, X + (Y + 1) * Side};
		bool Wet[4]; uint8 Mask = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const int32 Sample = Indices[Index];
			Wet[Index] = Kind[Sample] != 0 && Water[Sample] != MIN_int32 && Water[Sample] >= Ground[Sample];
			if (Wet[Index]) Mask |= 1u << Index;
		}
		if (!Mask) return;
		const int32 FirstWet = Wet[0] ? 0 : Wet[1] ? 1 : Wet[2] ? 2 : 3;
		const uint8 MaterialVariant = Kind[Indices[FirstWet]];
		const FVector Corners[4] = {
			FVector(X * Step, Y * Step, Wet[0] ? Water[Indices[0]] + 1.0 : 0.0),
			FVector((X + 1) * Step, Y * Step, Wet[1] ? Water[Indices[1]] + 1.0 : 0.0),
			FVector((X + 1) * Step, (Y + 1) * Step, Wet[2] ? Water[Indices[2]] + 1.0 : 0.0),
			FVector(X * Step, (Y + 1) * Step, Wet[3] ? Water[Indices[3]] + 1.0 : 0.0)};
		if (Mask == 15)
		{
			Visit(MakeArrayView(Corners, 4), MaterialVariant, true);
			return;
		}
		const auto Boundary = [&Corners](const int32 WetIndex, const int32 DryIndex)
		{
			const FVector& A = Corners[WetIndex]; const FVector& B = Corners[DryIndex];
			return FVector((A.X + B.X) * .5, (A.Y + B.Y) * .5, A.Z);
		};
		for (int32 Start = 0; Start < 4; ++Start)
		{
			if (!Wet[Start] || Wet[(Start + 3) & 3]) continue;
			TArray<FVector, TInlineAllocator<6>> Polygon;
			Polygon.Add(Boundary(Start, (Start + 3) & 3));
			int32 Corner = Start;
			for (int32 Run = 0; Run < 4 && Wet[Corner]; ++Run)
			{
				Polygon.Add(Corners[Corner]);
				const int32 Next = (Corner + 1) & 3;
				if (!Wet[Next]) { Polygon.Add(Boundary(Corner, Next)); break; }
				Corner = Next;
			}
			Visit(MakeArrayView(Polygon.GetData(), Polygon.Num()), MaterialVariant, false);
		}
	}

	template<typename Visitor>
	void VisitCellTriangles(const int32 Side, const int32 Step, TConstArrayView<int32> Ground,
		TConstArrayView<int32> Water, TConstArrayView<uint8> Kind, const int32 X, const int32 Y, Visitor&& Visit)
	{
		VisitCellPolygons(Side, Step, Ground, Water, Kind, X, Y,
			[&Visit](TConstArrayView<FVector> Polygon, const uint8 Variant, const bool)
			{
				for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
					Visit(Polygon[0], Polygon[Index + 1], Polygon[Index], Variant);
			});
	}
}
