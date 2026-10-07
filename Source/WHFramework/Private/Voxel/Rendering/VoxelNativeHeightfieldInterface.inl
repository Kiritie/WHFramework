// 包含于 Renderer 私有命名空间。所有坐标及水边缘来自实际冻结构建版本。
struct FNativeHeightfieldDistantSlice
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> Geometry;
	TArray<int32> Indices;
	int32 TileOffsets[257] = {};
};

struct FNativeHeightfieldInputs final : FVoxelTaskCustomPayload
{
	FIntVector Section = FIntVector::ZeroValue;
	uint8 Face = 0;
	FVoxelFineOwner FineOwner;
	FVoxelFineContentIdentity FineContent;
	FVoxelFineOwner CoarseOwner;
	FVoxelFineContentIdentity CoarseContent;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FineGeometry;
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> CoarseGeometry;
	TSharedPtr<const FNativeHeightfieldDistantSlice, ESPMode::ThreadSafe> DistantSlice;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	virtual uint64 GetAllocatedBytes() const override { return VoxelBoundedAllocation::WorkerSharedBytes<FNativeHeightfieldInputs>(); }
};

namespace NativeHeightfieldInterface
{
	using FPolygon = TArray<FVector2D, TInlineAllocator<16>>;
	using FPolygons = TArray<FPolygon, TInlineAllocator<32>>;
	struct FWaterRegion { FPolygon Polygon; uint8 Variant = 0; };

	bool AppendPoint(FPolygon& Polygon, const FVector2D& Point)
	{
		if (Polygon.Num() == 16) return false;
		Polygon.Add(Point); return true;
	}

	bool Split(const FPolygon& Input, const FVector2D& Normal, const double Distance, FPolygon& Inside, FPolygon& Outside)
	{
		Inside.Reset(); Outside.Reset();
		for (int32 Index = 0; Index < Input.Num(); ++Index)
		{
			const auto& A = Input[Index]; const auto& B = Input[(Index + 1) % Input.Num()];
			const double DA = FVector2D::DotProduct(Normal, A) - Distance;
			const double DB = FVector2D::DotProduct(Normal, B) - Distance;
			if (!AppendPoint(DA >= 0 ? Inside : Outside, A)) return false;
			if ((DA >= 0) != (DB >= 0))
			{
				const FVector2D Crossing = FMath::Lerp(A, B, DA / (DA - DB));
				if (!AppendPoint(Inside, Crossing) || !AppendPoint(Outside, Crossing)) return false;
			}
		}
		return true;
	}

	double Area(const FPolygon& Polygon)
	{
		double Sum = 0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const auto& A = Polygon[Index]; const auto& B = Polygon[(Index + 1) % Polygon.Num()];
			Sum += A.X * B.Y - B.X * A.Y;
		}
		return Sum * .5;
	}

	bool Clip(FPolygon& Polygon, const FVector2D& Normal, const double Distance)
	{
		FPolygon Inside, Outside;
		if (!Split(Polygon, Normal, Distance, Inside, Outside)) return false;
		Polygon = MoveTemp(Inside); return true;
	}

	bool Subtract(FPolygons& Input, const FPolygon& Cut)
	{
		if (Cut.Num() < 3 || FMath::Abs(Area(Cut)) < 1.e-12) return true;
		FPolygons Output;
		const double Sign = Area(Cut) >= 0 ? 1.0 : -1.0;
		for (const FPolygon& Source : Input)
		{
			FPolygon Remaining = Source;
			for (int32 Edge = 0; Edge < Cut.Num() && Remaining.Num() >= 3; ++Edge)
			{
				const auto& A = Cut[Edge]; const auto& B = Cut[(Edge + 1) % Cut.Num()];
				const FVector2D Normal((A.Y - B.Y) * Sign, (B.X - A.X) * Sign);
				FPolygon Inside, Outside;
				if (!Split(Remaining, Normal, FVector2D::DotProduct(Normal, A), Inside, Outside)) return false;
				if (Outside.Num() >= 3 && FMath::Abs(Area(Outside)) > 1.e-12)
				{
					if (Output.Num() == 32) return false;
					Output.Add(MoveTemp(Outside));
				}
				Remaining = MoveTemp(Inside);
			}
		}
		Input = MoveTemp(Output); return true;
	}

	FPolygon Rectangle(const double MinU, const double MaxU, const double MinV, const double MaxV)
	{
		return {{MinU, MinV}, {MaxU, MinV}, {MaxU, MaxV}, {MinU, MaxV}};
	}

	bool ClipRectangle(FPolygon& Polygon, const FPolygon& Rect)
	{
		return Clip(Polygon, {1, 0}, Rect[0].X) && Clip(Polygon, {-1, 0}, -Rect[2].X) &&
			Clip(Polygon, {0, 1}, Rect[0].Y) && Clip(Polygon, {0, -1}, -Rect[2].Y);
	}

	bool Occludes(const FVoxelBlockState A, const FVoxelRuntimeDefinition* DefinitionA,
		const FVoxelBlockState B, const FVoxelRuntimeDefinition* DefinitionB)
	{
		return DefinitionB && (DefinitionB->bOccludes || (DefinitionA && A.TypeId == B.TypeId &&
			(DefinitionA->RenderGroup == EVoxelRenderGroup::Water || DefinitionA->RenderGroup == EVoxelRenderGroup::Translucent)));
	}

	bool BuildTile(const FVoxelTerrainRenderAtom& Fine, const FVoxelTerrainRenderCoarse& Coarse,
		const uint8 Face, const int32 Tile, const FVoxelGenerationRuntimeConfig& Config,
		const FVoxelRegistrySnapshot& Registry, const FVoxelShapeRegistry& Shapes,
		const double TextureStretch, TConstArrayView<int32> DistantIndices, FVoxelSectionMeshResult& Output, FString& Error,
		const bool bCount, const FVoxelSectionMeshResult* Plan = nullptr)
	{
		const auto& Source = *Coarse.Geometry->Source;
		const auto Surface = Source.FrozenSurface; const auto Macro = Source.FrozenMacro;
		if ((!Surface && !Macro) || !Fine.FineGeometry || !Fine.FineGeometry->Boundary) return false;
		const int32 Side = Surface ? Surface->Side : Macro->Side, Step = Surface ? Surface->Step : Macro->Step;
		const FIntPoint Origin = Surface ? Surface->GetBuildOrigin() : Macro->GetBuildOrigin();
		const TConstArrayView<int32> Ground = Surface ? MakeArrayView(Surface->GroundZ) : MakeArrayView(Macro->Height);
		const TConstArrayView<uint16> Materials = Surface ? MakeArrayView(Surface->SurfaceMaterial) : MakeArrayView(Macro->SurfaceClass);
		const TConstArrayView<FVoxelDistantCell> DistantCells = Surface ? MakeArrayView(Surface->DistantCells) : MakeArrayView(Macro->DistantCells);
		const auto WaterData = Source.FrozenWater;
		const TConstArrayView<int32> WaterGround = Macro ? MakeArrayView(Macro->Height) : WaterData ? MakeArrayView(WaterData->GroundZ) : TConstArrayView<int32>();
		const TConstArrayView<int32> Water = Macro ? MakeArrayView(Macro->WaterHeight) : WaterData ? MakeArrayView(WaterData->WaterZ) : TConstArrayView<int32>();
		const TConstArrayView<uint8> Kind = Macro ? MakeArrayView(Macro->WaterKind) : WaterData ? MakeArrayView(WaterData->WaterKind) : TConstArrayView<uint8>();
		if (Side < 2 || Step <= 0 || Ground.Num() != Side * Side || Materials.Num() != Ground.Num() ||
			(!Water.IsEmpty() && (Water.Num() != Ground.Num() || WaterGround.Num() != Ground.Num() || Kind.Num() != Ground.Num()))) return false;
		const int32 Axis = Face / 2, U = Axis == 0 ? 1 : 0, V = Axis == 2 ? 1 : 2;
		const FVector FineMin = FVector(Fine.Section) * 16.0;
		const double Plane = FineMin[Axis] + ((Face & 1) ? 0.0 : 16.0);
		FIntVector FineCell = Fine.Section * 16;
		FineCell[Axis] += (Face & 1) ? 0 : 15; FineCell[U] += Tile % 16; FineCell[V] += Tile / 16;
		FVoxelBlockState FineState;
		if (!Fine.FineGeometry->Boundary->SampleAtomFace(Fine.Section, Face, false, FineCell, FineState)) return false;
		const auto* FineDefinition = FineState.IsAir() ? nullptr : Registry.Find(FineState.TypeId);
		const auto* FineShape = FineDefinition ? Shapes.Find(FineDefinition->Shape, FineState.State) : nullptr;
		if (!FineState.IsAir() && (!FineDefinition || !FineShape)) return false;
		FVector Sample(FineCell); Sample += FVector(.5); Sample[Axis] = Plane + ((Face & 1) ? -1.e-6 : 1.e-6);
		const int32 CellX = FMath::FloorToInt((Sample.X - Origin.X) / Step), CellY = FMath::FloorToInt((Sample.Y - Origin.Y) / Step);
		if (CellX < 0 || CellY < 0 || CellX >= Side - 1 || CellY >= Side - 1) return false;
		const int32 GroundIndex = CellX + CellY * Side;
		FVoxelBlockState GroundState;
		if (!Config.ToRuntime(Materials[GroundIndex], GroundState)) return false;
		const auto* GroundDefinition = GroundState.IsAir() ? nullptr : Registry.Find(GroundState.TypeId);
		const double Height = !GroundDefinition || Ground[GroundIndex] == MIN_int32 ? -MAX_dbl : Ground[GroundIndex] + 1.0 + (Surface ? -.02 : -.05);
		const FVoxelRuntimeDefinition* WaterDefinition = nullptr;
		for (const auto& Definition : Registry.Definitions) if (Definition.RenderGroup == EVoxelRenderGroup::Water) { WaterDefinition = &Definition; break; }
		FVoxelBlockState WaterState; if (WaterDefinition) WaterState.TypeId = WaterDefinition->TypeId;
		FVoxelSectionMeshResult Mesh;
		const int32 BatchBound = 12 * Registry.Definitions.Num();
		Mesh.Batches.Reserve(Plan ? Plan->Batches.Num() : BatchBound);
		if (Plan) for (const auto& Entry : Plan->Batches)
		{
			auto& Batch = Mesh.Batches.AddDefaulted_GetRef();
			Batch.Group = Entry.Group; Batch.Bank = Entry.Bank; Batch.MaterialVariant = Entry.MaterialVariant; Batch.Mesh.ReservePlan(Entry.Mesh);
		}
		const auto Emit = [&](const FPolygon& Polygon, const FVoxelBlockState State, const FVoxelRuntimeDefinition* Definition,
			const uint8 OutputFace, const int32 SampleStep, const uint8 Variant) -> bool
		{
			if (!Definition || Polygon.Num() < 3 || FMath::Abs(Area(Polygon)) < 1.e-12) return true;
			const uint8 MaterialFace = Variant > 0 && Definition->RenderGroup == EVoxelRenderGroup::Water ? 4 :
				FVoxelShapeRegistry::RotateFace(OutputFace, uint8((4 - (State.State & 3)) & 3));
			const auto& Texture = Definition->Face(State.State, MaterialFace);
			auto* Batch = Mesh.Batches.FindByPredicate([&](const auto& Entry)
			{ return Entry.Group == Definition->RenderGroup && Entry.Bank == Texture.Bank && Entry.MaterialVariant == Variant; });
			if (!Batch)
			{
				if (Plan || Mesh.Batches.Num() == BatchBound) return false;
				Batch = &Mesh.Batches.AddDefaulted_GetRef(); Batch->Group = Definition->RenderGroup; Batch->Bank = Texture.Bank; Batch->MaterialVariant = Variant;
			}
			auto& Buffers = Batch->Mesh;
			const int32 Indices = (Polygon.Num() - 2) * 3;
			if (bCount) { Buffers.PlannedVertices += Polygon.Num(); Buffers.PlannedIndices += Indices; return true; }
			if (Buffers.Vertices.Num() + Polygon.Num() > Buffers.PlannedVertices || Buffers.Triangles.Num() + Indices > Buffers.PlannedIndices) return false;
			const int32 Base = Buffers.Vertices.Num();
			FVector Normal(0); Normal[Axis] = (OutputFace & 1) ? -1.0 : 1.0;
			FVector Tangent(0); Tangent[U] = 1.0;
			const double Period = FMath::Clamp(double(SampleStep), 1.0, TextureStretch);
			for (const auto& Point : Polygon)
			{
				FVector Vertex(0); Vertex[Axis] = Plane; Vertex[U] = Point.X; Vertex[V] = Point.Y; Vertex -= FineMin;
				Buffers.Vertices.Add(Vertex); Buffers.Normals.Add(Normal);
				Buffers.UV0.Add(FVector2D(Vertex[U], Axis == 2 ? Vertex[V] : -Vertex[V]) / Period);
				Buffers.UV1.Add({double(Texture.Layer), double(Texture.Frames)}); Buffers.UV2.Add({double(Texture.FPS), 0});
				Buffers.Colors.Add(Texture.Tint); Buffers.Tangents.Add(FProcMeshTangent(Tangent, false));
			}
			const bool Reverse = ((Axis != 1) != ((OutputFace & 1) == 0)) != (Area(Polygon) < 0);
			for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
				Buffers.Triangles.Append({Base, Base + (Reverse ? Index : Index + 1), Base + (Reverse ? Index + 1 : Index)});
			return true;
		};
		const FPolygon Rect = Rectangle(FineCell[U], FineCell[U] + 1.0, FineCell[V], FineCell[V] + 1.0);
		// 直接裁切实际边界多边形；斜坡的三角边界不能折算为包围矩形。
		FPolygons ShapeCoverage;
		if (FineShape) for (const auto& Quad : FineShape->Quads)
		{
			if (!Quad.bBoundary || Quad.Face != Face) continue;
			FPolygon Polygon;
			for (const auto& Vertex : Quad.Vertices)
			{
				const FVector2D Point(FineCell[U] + Vertex[U], FineCell[V] + Vertex[V]);
				if (!Polygon.IsEmpty() && Polygon.Last().Equals(Point, 1.e-12)) continue;
				if (!AppendPoint(Polygon, Point)) return false;
			}
			if (Polygon.Num() > 1 && Polygon[0].Equals(Polygon.Last(), 1.e-12)) Polygon.Pop(EAllowShrinking::No);
			if (!ClipRectangle(Polygon, Rect)) return false;
			if (Polygon.Num() < 3 || FMath::Abs(Area(Polygon)) < 1.e-12) continue;
			FPolygons Added; Added.Add(MoveTemp(Polygon));
			for (const auto& Existing : ShapeCoverage)
			{
				if (!Subtract(Added, Existing)) { Error = TEXT("Oversized: Native shape union fragments"); return false; }
				if (Added.IsEmpty()) break;
			}
			if (ShapeCoverage.Num() + Added.Num() > 32) { Error = TEXT("Oversized: Native shape union fragments"); return false; }
			for (auto& Part : Added) ShapeCoverage.Add(MoveTemp(Part));
		}
		{
			FPolygon GroundPolygon = Rect;
			if (Axis < 2) { if (!Clip(GroundPolygon, {0, -1}, -Height)) return false; }
			else if (Sample.Z > Height) GroundPolygon.Reset();
			{
				FPolygons VisibleGround; VisibleGround.Add(GroundPolygon);
				if (Occludes(GroundState, GroundDefinition, FineState, FineDefinition))
					for (const auto& Part : ShapeCoverage) if (!Subtract(VisibleGround, Part)) return false;
				for (const auto& Polygon : VisibleGround) if (!Emit(Polygon, GroundState, GroundDefinition, Face ^ 1, Step, 0)) return false;
			}
			FPolygons FineRegions = ShapeCoverage;
			if (Occludes(FineState, FineDefinition, GroundState, GroundDefinition) && !Subtract(FineRegions, GroundPolygon)) return false;
			TArray<FPolygon, TInlineAllocator<8>> WaterRegions;
			bool bWaterValid = true;
			if (!Water.IsEmpty() && WaterDefinition)
				VoxelHeightfieldWater::VisitCellTriangles(Side, Step, WaterGround, Water, Kind, CellX, CellY,
					[&](FVector A, FVector B, FVector C, const uint8 Variant)
					{
						if (!bWaterValid) return;
						A += FVector(Origin.X, Origin.Y, 0); B += FVector(Origin.X, Origin.Y, 0); C += FVector(Origin.X, Origin.Y, 0);
						FPolygon WaterPolygon;
						if (Axis == 2)
						{
							if (Sample.Z <= Height) return;
							const FVector Triangle[3] = {A, B, C}; TArray<FVector, TInlineAllocator<4>> Clipped;
							for (int32 Edge = 0; Edge < 3; ++Edge)
							{
								const auto& P = Triangle[Edge]; const auto& Q = Triangle[(Edge + 1) % 3];
								if (P.Z >= Sample.Z) Clipped.Add(P);
								if ((P.Z >= Sample.Z) != (Q.Z >= Sample.Z)) Clipped.Add(FMath::Lerp(P, Q, (Plane - P.Z) / (Q.Z - P.Z)));
							}
							for (const auto& Point : Clipped) WaterPolygon.Add({Point.X, Point.Y});
						}
						else
						{
							const FVector Triangle[3] = {A, B, C}; TArray<FVector, TInlineAllocator<6>> Hits;
							const double Low = FMath::Min3(A[Axis], B[Axis], C[Axis]), High = FMath::Max3(A[Axis], B[Axis], C[Axis]);
							if (Sample[Axis] < Low || Sample[Axis] > High) return;
							for (int32 Edge = 0; Edge < 3; ++Edge)
							{
								const auto& P = Triangle[Edge]; const auto& Q = Triangle[(Edge + 1) % 3];
								if (FMath::IsNearlyEqual(P[Axis], Plane, 1.e-9)) Hits.Add(P);
								if ((P[Axis] < Plane && Q[Axis] > Plane) || (P[Axis] > Plane && Q[Axis] < Plane)) Hits.Add(FMath::Lerp(P, Q, (Plane - P[Axis]) / (Q[Axis] - P[Axis])));
							}
							if (Hits.Num() < 2) return;
							Hits.Sort([U](const auto& P, const auto& Q) { return P[U] < Q[U]; });
							const auto& P = Hits[0]; const auto& Q = Hits.Last(); if (Q[U] - P[U] < 1.e-9) return;
							const double Bottom = FMath::Max(Height, Rect[0].Y);
							const double Slope = (Q.Z - P.Z) / (Q[U] - P[U]);
							WaterPolygon = Rect;
							if (!Clip(WaterPolygon, {1, 0}, P[U]) || !Clip(WaterPolygon, {-1, 0}, -Q[U]) ||
								!Clip(WaterPolygon, {Slope, -1}, Slope * P[U] - P.Z) || !Clip(WaterPolygon, {0, 1}, Bottom))
							{ bWaterValid = false; return; }
						}
						if (!ClipRectangle(WaterPolygon, Rect)) { bWaterValid = false; return; }
						if (WaterPolygon.Num() >= 3 && FMath::Abs(Area(WaterPolygon)) > 1.e-12)
						{
							if (WaterRegions.Num() == 8) { bWaterValid = false; return; }
							WaterRegions.Add(WaterPolygon);
						}
						{
							FPolygons VisibleWater; VisibleWater.Add(WaterPolygon);
							if (Occludes(WaterState, WaterDefinition, FineState, FineDefinition))
								for (const auto& Part : ShapeCoverage) if (!Subtract(VisibleWater, Part)) { bWaterValid = false; return; }
							for (const int32 Index : DistantIndices)
							{
								if (!DistantCells.IsValidIndex(Index)) { bWaterValid = false; return; }
								const auto& Cell = DistantCells[Index];
								if (Sample[Axis] < Cell.Min[Axis] || Sample[Axis] > Cell.Max[Axis] ||
									Cell.State.TypeId == WaterState.TypeId ||
									!Occludes(WaterState, WaterDefinition, Cell.State, Registry.Find(Cell.State.TypeId))) continue;
								const FPolygon Cut = Rectangle(Cell.Min[U], Cell.Max[U], Cell.Min[V], Cell.Max[V]);
								if (!Subtract(VisibleWater, Cut)) { bWaterValid = false; return; }
								if (VisibleWater.IsEmpty()) break;
							}
							for (const auto& Polygon : VisibleWater) bWaterValid &= Emit(Polygon, WaterState, WaterDefinition, Face ^ 1, Step, Variant);
						}
						if (Occludes(FineState, FineDefinition, WaterState, WaterDefinition)) bWaterValid &= Subtract(FineRegions, WaterPolygon);
					});
			if (!bWaterValid) return false;
			// 远景体积实际按 Box 发射。截面沿用同一冻结 Box 和状态，不能折算成地表高度。
			for (const int32 DistantIndex : DistantIndices)
			{
				if (!DistantCells.IsValidIndex(DistantIndex)) return false;
				const auto& Cell = DistantCells[DistantIndex];
				if (Sample[Axis] < Cell.Min[Axis] || Sample[Axis] > Cell.Max[Axis]) continue;
				FPolygon DistantPolygon = Rectangle(Cell.Min[U], Cell.Max[U], Cell.Min[V], Cell.Max[V]);
				if (!ClipRectangle(DistantPolygon, Rect)) return false;
				const auto* Definition = Registry.Find(Cell.State.TypeId);
				if (!Definition) return false;
				{
					FPolygons Visible; Visible.Add(DistantPolygon);
					if (Occludes(Cell.State, Definition, FineState, FineDefinition))
						for (const auto& Part : ShapeCoverage) if (!Subtract(Visible, Part)) return false;
					if (Occludes(Cell.State, Definition, GroundState, GroundDefinition) && !Subtract(Visible, GroundPolygon)) return false;
					if (Occludes(Cell.State, Definition, WaterState, WaterDefinition))
						for (const auto& WaterPolygon : WaterRegions) if (!Subtract(Visible, WaterPolygon)) return false;
					// 冻结数组顺序为相同平面内重叠实体的稳定承载顺序，保留一份截面。
					for (const int32 PreviousIndex : DistantIndices)
					{
						if (PreviousIndex == DistantIndex) break;
						const auto& Previous = DistantCells[PreviousIndex];
						if (Sample[Axis] < Previous.Min[Axis] || Sample[Axis] > Previous.Max[Axis] ||
							!Occludes(Cell.State, Definition, Previous.State, Registry.Find(Previous.State.TypeId))) continue;
						const FPolygon Cut = Rectangle(Previous.Min[U], Previous.Max[U], Previous.Min[V], Previous.Max[V]);
						if (!Subtract(Visible, Cut)) return false;
						if (Visible.IsEmpty()) break;
					}
					for (const auto& Polygon : Visible) if (!Emit(Polygon, Cell.State, Definition, Face ^ 1, Step, 0)) return false;
				}
				if (Occludes(FineState, FineDefinition, Cell.State, Definition) && !Subtract(FineRegions, DistantPolygon)) return false;
			}
			for (const auto& Polygon : FineRegions) if (!Emit(Polygon, FineState, FineDefinition, Face, 1, 0)) return false;
		}
		if (!Mesh.ValidateAndCache()) { Error = TEXT("Invalid Native Fine/Heightfield interface"); return false; }
		Output = MoveTemp(Mesh); return true;
	}
}
