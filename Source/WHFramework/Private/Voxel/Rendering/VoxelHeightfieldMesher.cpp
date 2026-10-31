#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Geometry/VoxelFaceMerger.h"
#include "Voxel/Rendering/VoxelHeightfieldWaterTriangles.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"

namespace
{
	thread_local uint64* CountedMeshBytes = nullptr;
	thread_local FVoxelHeightfieldMesher::FCapacityScope* CapacityScope = nullptr;
	constexpr uint64 MeshVertexBytes = sizeof(FVector) * 2 + sizeof(FVector2D) * 3 +
		sizeof(FLinearColor) + sizeof(FProcMeshTangent);
	constexpr uint8 FacePositiveX = 0;
	constexpr uint8 FaceNegativeX = 1;
	constexpr uint8 FacePositiveY = 2;
	constexpr uint8 FaceNegativeY = 3;
	constexpr uint8 FacePositiveZ = 4;
	constexpr uint8 FaceNegativeZ = 5;
	constexpr uint8 TopMaterialFace = 4;

	int32 VertexIndex(
		const int32 InX,
		const int32 InY,
		const int32 InSide)
	{
		return
			InX +
			InY *
				InSide;
	}

	bool ResolveFace(
		const FVoxelBlockState InState,
		const uint8 InFaceIndex,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelRuntimeDefinition*& OutDefinition,
		const FVoxelRuntimeFaceRef*& OutFace,
		FString& OutError)
	{
		OutDefinition = InRegistry.Find(InState.TypeId);

		if (!OutDefinition)
		{
			OutError = TEXT("Blocky heightfield material is missing from registry");
			return false;
		}

		const uint8 MaterialFace = FVoxelShapeRegistry::RotateFace(InFaceIndex, uint8((4 - (InState.State & 3)) & 3));
		OutFace = &OutDefinition->Face(InState.State, MaterialFace);
		return true;
	}

	void AppendWaterPolygon(FVoxelMeshBuffers& InOutMesh,
		TConstArrayView<FVector> InVertices, const FVoxelRuntimeFaceRef& InFace)
	{
		if (CapacityScope && CapacityScope->Count(InOutMesh, InVertices.Num(), FMath::Max(0, InVertices.Num() - 2) * 3)) return;
		if (CountedMeshBytes)
		{
			*CountedMeshBytes += uint64(InVertices.Num()) * MeshVertexBytes +
				uint64(FMath::Max(0, InVertices.Num() - 2)) * 3 * sizeof(int32);
			return;
		}
		const int32 Base = InOutMesh.Vertices.Num();
		for (const FVector& Vertex : InVertices)
		{
			InOutMesh.Vertices.Add(Vertex);
			InOutMesh.Normals.Add(FVector::UpVector);
			InOutMesh.UV0.Add(FVector2D(Vertex.X, Vertex.Y));
			InOutMesh.UV1.Add(FVector2D(InFace.Layer, InFace.Frames));
			InOutMesh.UV2.Add(FVector2D(InFace.FPS, 0.0));
			InOutMesh.Colors.Add(InFace.Tint);
			InOutMesh.Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		for (int32 Index = 1; Index + 1 < InVertices.Num(); ++Index)
		{
			InOutMesh.Triangles.Append({Base, Base + Index + 1, Base + Index});
		}
	}
}

FVoxelHeightfieldMesher::FCapacityScope::FCapacityScope(const bool bInCount,
	TArray<FVoxelRenderBatch>& InPlan, const int32 InBatchLimit)
	: Previous(CapacityScope), Plan(InPlan), BatchLimit(InBatchLimit), bCounting(bInCount)
{
	if (bCounting) { Plan.Reset(); Plan.Reserve(BatchLimit); }
	CapacityScope = this;
}
FVoxelHeightfieldMesher::FCapacityScope::~FCapacityScope()
{
	CapacityScope = Previous;
}
bool FVoxelHeightfieldMesher::FCapacityScope::IsValid() const
{
	return bValid && (bCounting || NextBatch == Plan.Num());
}
void FVoxelHeightfieldMesher::FCapacityScope::Bind(FVoxelRenderBatch& InBatch)
{
	if (!bValid || NextBatch >= BatchLimit) { bValid = false; return; }
	InBatch.Mesh.CapacityPlanIndex = NextBatch++;
	if (bCounting)
	{
		auto& Entry = Plan.AddDefaulted_GetRef();
		Entry.Group = InBatch.Group; Entry.Bank = InBatch.Bank; Entry.MaterialVariant = InBatch.MaterialVariant;
		return;
	}
	if (!Plan.IsValidIndex(InBatch.Mesh.CapacityPlanIndex)) { bValid = false; return; }
	const auto& Entry = Plan[InBatch.Mesh.CapacityPlanIndex];
	if (Entry.Group != InBatch.Group || Entry.Bank != InBatch.Bank || Entry.MaterialVariant != InBatch.MaterialVariant)
	{ bValid = false; return; }
	InBatch.Mesh.ReservePlan(Entry.Mesh);
}
bool FVoxelHeightfieldMesher::FCapacityScope::Count(FVoxelMeshBuffers& InMesh, const int32 InVertices, const int32 InIndices)
{
	if (!bValid) return true;
	if (!bCounting)
	{
		if (InMesh.Vertices.Num() + InVertices > InMesh.PlannedVertices ||
			InMesh.Triangles.Num() + InIndices > InMesh.PlannedIndices)
		{ bValid = false; return true; }
		return false;
	}
	if (!Plan.IsValidIndex(InMesh.CapacityPlanIndex)) { bValid = false; return true; }
	auto& Entry = Plan[InMesh.CapacityPlanIndex].Mesh;
	Entry.PlannedVertices += InVertices; Entry.PlannedIndices += InIndices;
	InMesh.PlannedVertices += InVertices; InMesh.PlannedIndices += InIndices;
	return true;
}

bool FVoxelHeightfieldMesher::FCapacityScope::PrepareBatches(FVoxelSectionMeshResult& InMesh)
{
	if (!bValid || InMesh.Batches.Num() >= BatchLimit) { bValid = false; return false; }
	if (InMesh.Batches.Max() < BatchLimit) InMesh.Batches.Reserve(BatchLimit);
	return true;
}

FVoxelRenderBatch& FVoxelHeightfieldMesher::FCapacityScope::RejectedBatch()
{
	bValid = false;
	return Rejected;
}

FVoxelHeightfieldMesher::FCountScope::FCountScope(uint64* InBytes)
	: Previous(CountedMeshBytes)
{
	CountedMeshBytes = InBytes;
}

FVoxelHeightfieldMesher::FCountScope::~FCountScope()
{
	CountedMeshBytes = Previous;
}

FVoxelRenderBatch& FVoxelHeightfieldMesher::FindOrAddBatch(
	FVoxelSectionMeshResult& InOutMesh,
	const EVoxelRenderGroup InGroup,
	const uint16 InBank,
	const uint8 InVariant)
{
	for (FVoxelRenderBatch& Batch :
		InOutMesh.Batches)
	{
		if (Batch.Group ==
				InGroup &&
			Batch.Bank ==
				InBank && Batch.MaterialVariant == InVariant)
		{
			return Batch;
		}
	}

	if (CapacityScope && !CapacityScope->PrepareBatches(InOutMesh)) return CapacityScope->RejectedBatch();
	FVoxelRenderBatch& Batch =
		InOutMesh.Batches.
			AddDefaulted_GetRef();

	Batch.Group =
		InGroup;

	Batch.Bank =
		InBank;
	Batch.MaterialVariant = InVariant;
	if (CapacityScope) CapacityScope->Bind(Batch);

	return Batch;
}

void FVoxelHeightfieldMesher::AppendQuad(
	FVoxelMeshBuffers& InOutMesh,
	const FVector& InA,
	const FVector& InB,
	const FVector& InC,
	const FVector& InD,
	const FVoxelRuntimeFaceRef& InFace,
	const FLinearColor& InColor)
{
	if (CapacityScope && CapacityScope->Count(InOutMesh, 4, 6)) return;
	if (CountedMeshBytes)
	{
		*CountedMeshBytes += 4 * MeshVertexBytes + 6 * sizeof(int32);
		return;
	}
	const int32 Base =
		InOutMesh.Vertices.Num();

	const FVector NormalA =
		FVector::CrossProduct(
			InB - InA,
			InC - InA).
			GetSafeNormal();

	const FVector NormalB =
		FVector::CrossProduct(
			InC - InA,
			InD - InA).
			GetSafeNormal();

	const FVector Normal =
		(NormalA + NormalB).
			GetSafeNormal();

	const FVector TangentVector =
		(InB - InA).
			GetSafeNormal();

	const FVector Vertices[] =
	{
		InA,
		InB,
		InC,
		InD
	};

	const bool bTop = FMath::Abs(Normal.Z) >= FMath::Max(FMath::Abs(Normal.X), FMath::Abs(Normal.Y));
	const bool bAlongX = FMath::Abs(Normal.X) > FMath::Abs(Normal.Y);
	FVector2D Coordinates[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector& Position = Vertices[Index];
		Coordinates[Index] = bTop ? FVector2D(Position.X, Position.Y)
			: FVector2D(bAlongX ? Position.Y : Position.X, -Position.Z);
	}

	for (int32 Index = 0;
		Index < 4;
		++Index)
	{
		InOutMesh.Vertices.Add(
			Vertices[Index]);

		InOutMesh.Normals.Add(
			Normal);

		InOutMesh.UV0.Add(Coordinates[Index]);

		InOutMesh.UV1.Add(
			FVector2D(
				InFace.Layer,
				InFace.Frames));

		InOutMesh.UV2.Add(
			FVector2D(
				InFace.FPS,
				0.0));

		InOutMesh.Colors.Add(InFace.Tint * InColor);

		InOutMesh.Tangents.Add(
			FProcMeshTangent(
				TangentVector,
				false));
	}

	InOutMesh.Triangles.Append({
		Base,
		Base + 2,
		Base + 1,
		Base,
		Base + 3,
		Base + 2
	});
}

bool FVoxelHeightfieldMesher::BuildBlockyTerrain(
	const int32 InVertexSide,
	const int32 InStep,
	TConstArrayView<int32> InHeights,
	TConstArrayView<FVoxelBlockState> InMaterials,
	const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	TConstArrayView<uint8> InCoverage,
	const double InZBiasCells,
	const double InMaximumTextureStretchCells,
	TConstArrayView<FLinearColor> InTopTints, const FIntRect& InWindow)
{
	const FIntRect Window = InWindow.IsEmpty() ? FIntRect(0, 0, InVertexSide - 1, InVertexSide - 1) : InWindow;
	if (Window.Min.X < 0 || Window.Min.Y < 0 || Window.Max.X >= InVertexSide || Window.Max.Y >= InVertexSide)
	{ OutError = TEXT("Invalid heightfield execution window"); return false; }
	if (InVertexSide < 2 ||
		InStep <= 0 ||
		InHeights.Num() != InVertexSide * InVertexSide ||
		InMaterials.Num() != InHeights.Num() ||
		(!InCoverage.IsEmpty() && InCoverage.Num() != InHeights.Num()) ||
		(!InTopTints.IsEmpty() && InTopTints.Num() != InHeights.Num() && InTopTints.Num() != Window.Width() * Window.Height()))
	{
		OutError = TEXT("Invalid blocky heightfield input");
		return false;
	}

	FVoxelSectionMeshResult Mesh;
	const int32 CellSide = InVertexSide - 1;
	auto CellIndex =
		[InVertexSide](const int32 InX, const int32 InY)
		{
			return VertexIndex(InX, InY, InVertexSide);
		};

	auto TopZ =
		[&InHeights, &CellIndex, InZBiasCells](const int32 InX, const int32 InY)
		{
			return static_cast<double>(InHeights[CellIndex(InX, InY)]) + 1.0 + InZBiasCells;
		};

	auto CoverageColor =
		[&InCoverage, &CellIndex](const int32 InX, const int32 InY)
		{
			if (InCoverage.IsEmpty())
			{
				return FLinearColor::White;
			}

			const float Value = static_cast<float>(InCoverage[CellIndex(InX, InY)]) / 255.0f;
			return FLinearColor(Value, Value, Value, 1.0f);
		};

	auto TopTint =
		[&InTopTints, &CellIndex, &Window, InVertexSide](const int32 InX, const int32 InY)
		{
			if (InTopTints.IsEmpty()) return FLinearColor::White;
			return InTopTints[InTopTints.Num() == InVertexSide * InVertexSide ? CellIndex(InX, InY)
				: (FMath::Clamp(InX, Window.Min.X, Window.Max.X - 1) - Window.Min.X) + Window.Width() *
				(FMath::Clamp(InY, Window.Min.Y, Window.Max.Y - 1) - Window.Min.Y)];
		};

	auto AppendMaterialQuad =
		[&Mesh, &InConfig, &InRegistry, &InMaterials, &InHeights, &TopTint, &CellIndex, &OutError](
			const int32 InMaterialX,
			const int32 InMaterialY,
			const uint8 InFaceIndex,
			const FVector& InA,
			const FVector& InB,
			const FVector& InC,
			const FVector& InD,
			const FLinearColor& InColor)
		{
			if (InHeights[CellIndex(InMaterialX, InMaterialY)] == MIN_int32)
			{
				return true;
			}
			const FVoxelRuntimeDefinition* Definition = nullptr;
			const FVoxelRuntimeFaceRef* Face = nullptr;

			if (!ResolveFace(
					InMaterials[CellIndex(InMaterialX, InMaterialY)],
					InFaceIndex,
					InRegistry,
					Definition,
					Face,
					OutError))
			{
				return false;
			}

			FVoxelRenderBatch& Batch = FindOrAddBatch(Mesh, Definition->RenderGroup, Face->Bank);
			const FLinearColor Tint = InFaceIndex == FacePositiveZ
				? TopTint(InMaterialX, InMaterialY) : FLinearColor::White;
			AppendQuad(Batch.Mesh, InA, InB, InC, InD, *Face, InColor * Tint);
			return true;
		};

	// 固定栈上列游标覆盖矩形，不分配逐格 mask；大窗口按列带接续同一发射路径。
	constexpr int32 MaximumTopStripeWidth = 256;
	for (int32 StripeMinX = Window.Min.X; StripeMinX < Window.Max.X;)
	{
		const int32 StripeMaxX = StripeMinX + FMath::Min(MaximumTopStripeWidth, Window.Max.X - StripeMinX);
		int32 TopCoveredUntilY[MaximumTopStripeWidth] = {};
		int32 XSideCoveredUntilY[MaximumTopStripeWidth] = {};
		for (int32 Y = Window.Min.Y; Y < Window.Max.Y; ++Y)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}

			int32 YSideRunEndX = StripeMinX;
			for (int32 X = StripeMinX; X < StripeMaxX; ++X)
			{
				const double X0 = static_cast<double>(X * InStep);
				const double X1 = static_cast<double>((X + 1) * InStep);
				const double Y0 = static_cast<double>(Y * InStep);
				const double Y1 = static_cast<double>((Y + 1) * InStep);
				const double CurrentTop = TopZ(X, Y);
				const FLinearColor Color = CoverageColor(X, Y);

				if (TopCoveredUntilY[X - StripeMinX] <= Y)
				{
					const int32 CurrentIndex = CellIndex(X, Y);
					const FLinearColor Tint = TopTint(X, Y);
					const auto MatchesTop = [&](const int32 CandidateX, const int32 CandidateY)
					{
						const int32 Index = CellIndex(CandidateX, CandidateY);
						return TopCoveredUntilY[CandidateX - StripeMinX] <= Y &&
							InHeights[Index] == InHeights[CurrentIndex] && InMaterials[Index] == InMaterials[CurrentIndex] &&
							CoverageColor(CandidateX, CandidateY) == Color && TopTint(CandidateX, CandidateY) == Tint;
					};
					FIntRect Rectangle;
					if (!FVoxelFaceMerger::Grow(FIntPoint(X, Y), FIntPoint(StripeMaxX, Window.Max.Y), MatchesTop, Rectangle, InCancel))
					{
						OutError = TEXT("Canceled");
						return false;
					}
					const int32 RunEndX = Rectangle.Max.X;
					const int32 RunEndY = Rectangle.Max.Y;
					for (int32 CoveredX = X; CoveredX < RunEndX; ++CoveredX)
						TopCoveredUntilY[CoveredX - StripeMinX] = RunEndY;
					const double RunX1 = static_cast<double>(RunEndX * InStep);
					const double RunY1 = static_cast<double>(RunEndY * InStep);
					if (!AppendMaterialQuad(X, Y, FacePositiveZ,
						FVector(X0, Y0, CurrentTop), FVector(RunX1, Y0, CurrentTop),
						FVector(RunX1, RunY1, CurrentTop), FVector(X0, RunY1, CurrentTop), Color))
					{
						return false;
					}
				}

				if (X + 1 < CellSide && XSideCoveredUntilY[X - StripeMinX] <= Y &&
					InHeights[CellIndex(X, Y)] != MIN_int32 && InHeights[CellIndex(X + 1, Y)] != MIN_int32 &&
					InHeights[CellIndex(X, Y)] != InHeights[CellIndex(X + 1, Y)])
				{
					const double NeighborTop = TopZ(X + 1, Y);
					const int32 MaterialX = CurrentTop > NeighborTop ? X : X + 1;
					const FLinearColor WallColor = CoverageColor(MaterialX, Y);
					FIntRect Rectangle;
					if (!FVoxelFaceMerger::Grow(FIntPoint(Y, 0), FIntPoint(Window.Max.Y, 1),
						[&](const int32 CandidateY, const int32)
						{
							return InHeights[CellIndex(X, CandidateY)] == InHeights[CellIndex(X, Y)] &&
								InHeights[CellIndex(X + 1, CandidateY)] == InHeights[CellIndex(X + 1, Y)] &&
								InMaterials[CellIndex(MaterialX, CandidateY)] == InMaterials[CellIndex(MaterialX, Y)] &&
								CoverageColor(MaterialX, CandidateY) == WallColor;
						}, Rectangle, InCancel))
					{
						OutError = TEXT("Canceled");
						return false;
					}
					const int32 RunEndY = Rectangle.Max.X;
					XSideCoveredUntilY[X - StripeMinX] = RunEndY;
					const double WallY1 = static_cast<double>(RunEndY * InStep);

					if (CurrentTop > NeighborTop &&
						!AppendMaterialQuad(
							X, Y, FacePositiveX,
							FVector(X1, WallY1, CurrentTop), FVector(X1, Y0, CurrentTop),
							FVector(X1, Y0, NeighborTop), FVector(X1, WallY1, NeighborTop), WallColor))
					{
						return false;
					}

					if (NeighborTop > CurrentTop &&
						!AppendMaterialQuad(
							X + 1, Y, FaceNegativeX,
							FVector(X1, Y0, NeighborTop), FVector(X1, WallY1, NeighborTop),
							FVector(X1, WallY1, CurrentTop), FVector(X1, Y0, CurrentTop), WallColor))
					{
						return false;
					}
				}

				if (Y + 1 < CellSide && X >= YSideRunEndX && InHeights[CellIndex(X, Y)] != MIN_int32 &&
					InHeights[CellIndex(X, Y + 1)] != MIN_int32 &&
					InHeights[CellIndex(X, Y)] != InHeights[CellIndex(X, Y + 1)])
				{
					const double NeighborTop = TopZ(X, Y + 1);
					const int32 MaterialY = CurrentTop > NeighborTop ? Y : Y + 1;
					const FLinearColor WallColor = CoverageColor(X, MaterialY);
					FIntRect Rectangle;
					if (!FVoxelFaceMerger::Grow(FIntPoint(X, 0), FIntPoint(StripeMaxX, 1),
						[&](const int32 CandidateX, const int32)
						{
							return InHeights[CellIndex(CandidateX, Y)] == InHeights[CellIndex(X, Y)] &&
								InHeights[CellIndex(CandidateX, Y + 1)] == InHeights[CellIndex(X, Y + 1)] &&
								InMaterials[CellIndex(CandidateX, MaterialY)] == InMaterials[CellIndex(X, MaterialY)] &&
								CoverageColor(CandidateX, MaterialY) == WallColor;
						}, Rectangle, InCancel))
					{
						OutError = TEXT("Canceled");
						return false;
					}
					const int32 RunEndX = Rectangle.Max.X;
					YSideRunEndX = RunEndX;
					const double WallX1 = static_cast<double>(RunEndX * InStep);

					if (CurrentTop > NeighborTop &&
						!AppendMaterialQuad(
							X, Y, FacePositiveY,
							FVector(X0, Y1, CurrentTop), FVector(WallX1, Y1, CurrentTop),
							FVector(WallX1, Y1, NeighborTop), FVector(X0, Y1, NeighborTop), WallColor))
					{
						return false;
					}

					if (NeighborTop > CurrentTop &&
						!AppendMaterialQuad(
							X, Y + 1, FaceNegativeY,
							FVector(WallX1, Y1, NeighborTop), FVector(X0, Y1, NeighborTop),
							FVector(X0, Y1, CurrentTop), FVector(WallX1, Y1, CurrentTop), WallColor))
					{
						return false;
					}
				}
			}
		}
		StripeMinX = StripeMaxX;
	}

	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		if (!Batch.Mesh.Validate())
		{
			OutError = TEXT("Blocky heightfield mesh validation failed");
			return false;
		}
	}

	const double TexturePeriod = VoxelViewLod::TexturePeriodCells(InStep, InMaximumTextureStretchCells);
	for (FVoxelRenderBatch& OutputBatch : Mesh.Batches)
	{
		for (FVector2D& UV : OutputBatch.Mesh.UV0)
		{
			UV /= TexturePeriod;
		}
	}

	if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid heightfield mesh"); return false; }
	OutMesh = MoveTemp(Mesh);
	OutError.Reset();
	return true;
}

bool FVoxelHeightfieldMesher::AppendDistantCells(
	TConstArrayView<FVoxelDistantCell> InCells,
	const FIntPoint& InTileOrigin,
	const int32 InStep,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& InOutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const double InMaximumTextureStretchCells, const FIntRect& InWindow)
{
	const double TexturePeriod = VoxelViewLod::TexturePeriodCells(InStep, InMaximumTextureStretchCells);
	struct FDistantFaceKey
	{
		FVoxelRuntimeFaceRef Material;
		EVoxelRenderGroup Group = EVoxelRenderGroup::Opaque;
		bool operator==(const FDistantFaceKey& InOther) const { return Material == InOther.Material && Group == InOther.Group; }
	};
	TVoxelPlanarFaceMerger<FDistantFaceKey> Merger;
	const auto EmitFace = [&](const uint8 FaceIndex, const double Plane, const FBox2D& Rect, const FDistantFaceKey& Key)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		const int32 Axis = FaceIndex / 2;
		const int32 U = (Axis + 1) % 3;
		const int32 V = (Axis + 2) % 3;
		const int32 Corners[6][4] = {{2, 3, 0, 1}, {3, 2, 1, 0}, {1, 2, 3, 0}, {2, 1, 0, 3}, {0, 1, 2, 3}, {3, 2, 1, 0}};
		const double CoordinatesU[] = {Rect.Min.X, Rect.Max.X, Rect.Max.X, Rect.Min.X};
		const double CoordinatesV[] = {Rect.Min.Y, Rect.Min.Y, Rect.Max.Y, Rect.Max.Y};
		FVector Vertices[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Vertices[Index][Axis] = Plane;
			Vertices[Index][U] = CoordinatesU[Corners[FaceIndex][Index]];
			Vertices[Index][V] = CoordinatesV[Corners[FaceIndex][Index]];
		}
		FVoxelRenderBatch& Batch = FindOrAddBatch(InOutMesh, Key.Group, Key.Material.Bank);
		const int32 FirstUV = Batch.Mesh.UV0.Num();
		AppendQuad(Batch.Mesh, Vertices[0], Vertices[1], Vertices[2], Vertices[3], Key.Material);
		for (int32 Index = FirstUV; Index < Batch.Mesh.UV0.Num(); ++Index) Batch.Mesh.UV0[Index] /= TexturePeriod;
		return true;
	};
	for (const FVoxelDistantCell& Cell : InCells)
	{
		const FIntPoint CellPosition((Cell.Min.X - InTileOrigin.X) / InStep, (Cell.Min.Y - InTileOrigin.Y) / InStep);
		if (!InWindow.IsEmpty() && !InWindow.Contains(CellPosition)) continue;
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (Cell.State.IsAir() || Cell.Min.X >= Cell.Max.X ||
			Cell.Min.Y >= Cell.Max.Y || Cell.Min.Z >= Cell.Max.Z)
		{
			OutError = TEXT("Distant voxel cell has invalid bounds or state");
			return false;
		}
		const FVoxelRuntimeDefinition* Definition = InRegistry.Find(Cell.State.TypeId);
		if (!Definition)
		{
			OutError = TEXT("Distant voxel cell is missing from registry");
			return false;
		}
		const FVector Min(Cell.Min.X - InTileOrigin.X, Cell.Min.Y - InTileOrigin.Y, Cell.Min.Z);
		const FVector Max(Cell.Max.X - InTileOrigin.X, Cell.Max.Y - InTileOrigin.Y, Cell.Max.Z);
		const bool bOccludes = Definition->bOccludes && Definition->RenderGroup == EVoxelRenderGroup::Opaque;
		for (uint8 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			const int32 Axis = FaceIndex / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			const FDistantFaceKey Key{Definition->Face(Cell.State.State, FaceIndex), Definition->RenderGroup};
			if (!Merger.Add(FaceIndex, (FaceIndex & 1) ? Min[Axis] : Max[Axis],
				FBox2D(FVector2D(Min[U], Min[V]), FVector2D(Max[U], Max[V])), Key, bOccludes, EmitFace)) return false;
		}
	}
	if (!Merger.Flush(EmitFace)) return false;
	OutError.Reset();
	return true;
}

bool FVoxelHeightfieldMesher::BuildMacro(
	const FVoxelMacroTileData& InMacro,
	const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const double InMaximumTextureStretchCells, const FIntRect& InWindow)
{
	const FIntRect Window = InWindow.IsEmpty() ? FIntRect(0, 0, InMacro.Side - 1, InMacro.Side - 1) : InWindow;
	const int32 Count = InMacro.Side * InMacro.Side;
	if (InMacro.Side < 2 || InMacro.Step <= 0 ||
		InMacro.WaterHeight.Num() != Count || InMacro.Height.Num() != Count ||
		InMacro.WaterKind.Num() != Count ||
		InMacro.SurfaceStates.Num() != Count ||
		(!InMacro.ForestCoverage.IsEmpty() && InMacro.ForestCoverage.Num() != Count))
	{
		OutError = TEXT("Invalid macro water input");
		return false;
	}

	TArray<FLinearColor> TopTints;
	if (InConfig.Recipe->Settings.Ecology.Tree.bEnabled && !InMacro.ForestCoverage.IsEmpty())
	{
		FVoxelBlockState Leaves;
		const FVoxelRuntimeDefinition* LeafDefinition =
			InConfig.ToRuntime(InConfig.Recipe->Ecology.TreeLeaves, Leaves) && !Leaves.IsAir()
				? InRegistry.Find(Leaves.TypeId) : nullptr;
		if (!LeafDefinition)
		{
			OutError = TEXT("Macro forest material is missing from registry");
			return false;
		}
		FLinearColor LeafTint = LeafDefinition->Face(Leaves.State, TopMaterialFace).Tint;
		LeafTint.A = 1.0f;
		TopTints.Reserve(Window.Width() * Window.Height());
		TopTints.Init(FLinearColor::White, Window.Width() * Window.Height());
		for (int32 Y = Window.Min.Y; Y < Window.Max.Y; ++Y)
		for (int32 X = Window.Min.X; X < Window.Max.X; ++X)
		{
			const int32 Index = X + Y * InMacro.Side;
			if (InMacro.SurfaceStates[Index].TypeId == InConfig.Grass.TypeId &&
				(InMacro.WaterHeight[Index] == MIN_int32 || InMacro.WaterHeight[Index] < InMacro.Height[Index]))
			{
				TopTints[(X - Window.Min.X) + Window.Width() * (Y - Window.Min.Y)] = FMath::Lerp(FLinearColor::White, LeafTint,
					static_cast<float>(InMacro.ForestCoverage[Index]) / 255.0f);
			}
		}
	}
	FVoxelSectionMeshResult Mesh;
	if (!BuildBlockyTerrain(
		InMacro.Side, InMacro.Step, InMacro.Height, InMacro.SurfaceStates,
		InConfig, InRegistry, Mesh, OutError, InCancel, {}, -0.05,
		InMaximumTextureStretchCells, TopTints, Window))
	{
		return false;
	}

	FVoxelSectionMeshResult WaterMesh;
	if (!BuildWaterGrid(InMacro.Side, InMacro.Step, InMacro.Height, InMacro.WaterHeight,
		InMacro.WaterKind, InRegistry, WaterMesh, OutError, InCancel, InMaximumTextureStretchCells, Window)) return false;
	for (auto& Batch : WaterMesh.Batches)
		if (!Batch.Mesh.Triangles.IsEmpty() || Batch.Mesh.PlannedVertices > 0) Mesh.Batches.Add(MoveTemp(Batch));
	if (!AppendDistantCells(InMacro.DistantCells,
		InMacro.GetBuildOrigin(),
		InMacro.Step, InRegistry, Mesh, OutError, InCancel,
		InMaximumTextureStretchCells, Window))
	{
		return false;
	}

	if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid heightfield mesh"); return false; }
	OutMesh = MoveTemp(Mesh);
	OutError.Reset();
	return true;
}

bool FVoxelHeightfieldMesher::BuildWater(
	const FVoxelWaterSurfaceTileData& InWater,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const double InMaximumTextureStretchCells, const FIntRect& InWindow)
{
	return BuildWaterGrid(InWater.Side, InWater.Step, InWater.GroundZ, InWater.WaterZ,
		InWater.WaterKind, InRegistry, OutMesh, OutError, InCancel, InMaximumTextureStretchCells, InWindow);
}

bool FVoxelHeightfieldMesher::BuildWaterGrid(const int32 InSide, const int32 InStep,
	TConstArrayView<int32> InGroundZ, TConstArrayView<int32> InWaterZ,
	TConstArrayView<uint8> InWaterKind, const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh, FString& OutError, const TAtomic<bool>* InCancel,
	const double InMaximumTextureStretchCells, const FIntRect& InWindow)
{
	const FIntRect Window = InWindow.IsEmpty() ? FIntRect(0, 0, InSide - 1, InSide - 1) : InWindow;
	const int32 Count =
		InSide *
		InSide;

	if (InSide < 2 ||
		InStep <= 0 ||
		InWaterZ.Num() !=
			Count ||
		InGroundZ.Num() !=
			Count ||
		InWaterKind.Num() !=
			Count)
	{
		OutError =
			TEXT("Invalid continuous water input");

		return false;
	}

	const FVoxelRuntimeDefinition* WaterDefinition =
		nullptr;

	for (const FVoxelRuntimeDefinition& Definition :
		InRegistry.Definitions)
	{
		if (Definition.RenderGroup ==
			EVoxelRenderGroup::Water)
		{
			WaterDefinition =
				&Definition;

			break;
		}
	}

	if (!WaterDefinition)
	{
		OutError =
			TEXT("Runtime registry has no water material");

		return false;
	}

	const FVoxelRuntimeFaceRef& Face =
		WaterDefinition->Face(
			0,
			TopMaterialFace);

	FVoxelSectionMeshResult Mesh;



	constexpr int32 StripeWidth = 256;
	for (int32 StripeMinX = Window.Min.X; StripeMinX < Window.Max.X; StripeMinX += StripeWidth)
	{
		const int32 StripeMaxX = FMath::Min(Window.Max.X, StripeMinX + StripeWidth);
		int32 CoveredUntilY[StripeWidth];
		for (int32& CoveredY : CoveredUntilY) CoveredY = Window.Min.Y;
		for (int32 Y = Window.Min.Y; Y < Window.Max.Y; ++Y)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			for (int32 X = StripeMinX; X < StripeMaxX; ++X)
			{
				if (CoveredUntilY[X - StripeMinX] > Y) continue;
				const int32 First = X + Y * InSide;
				const int32 Height = InWaterZ[First];
				const uint8 Kind = InWaterKind[First];
				const auto MatchesFlatCell = [&](const int32 CandidateX, const int32 CandidateY)
				{
					if (!Kind || Height == MIN_int32 || CoveredUntilY[CandidateX - StripeMinX] > Y) return false;
					const int32 Index = CandidateX + CandidateY * InSide;
					for (const int32 Corner : {Index, Index + 1, Index + InSide, Index + InSide + 1})
						if (InWaterKind[Corner] != Kind || InWaterZ[Corner] != Height || Height < InGroundZ[Corner]) return false;
					return true;
				};
				if (MatchesFlatCell(X, Y))
				{
					FIntRect Rectangle;
					if (!FVoxelFaceMerger::Grow(FIntPoint(X, Y), FIntPoint(StripeMaxX, Window.Max.Y), MatchesFlatCell, Rectangle, InCancel))
					{
						OutError = TEXT("Canceled");
						return false;
					}
					const int32 EndX = Rectangle.Max.X;
					const int32 EndY = Rectangle.Max.Y;
					for (int32 CoveredX = X; CoveredX < EndX; ++CoveredX) CoveredUntilY[CoveredX - StripeMinX] = EndY;
					auto& Batch = FindOrAddBatch(Mesh, EVoxelRenderGroup::Water, Face.Bank, Kind);
					const double Top = double(Height) + 1.0;
					AppendQuad(Batch.Mesh, FVector(X * InStep, Y * InStep, Top), FVector(EndX * InStep, Y * InStep, Top),
						FVector(EndX * InStep, EndY * InStep, Top), FVector(X * InStep, EndY * InStep, Top), Face);
				}
				else
				{
					VoxelHeightfieldWater::VisitCellPolygons(InSide, InStep, InGroundZ, InWaterZ, InWaterKind, X, Y,
						[&Mesh, &Face](TConstArrayView<FVector> Polygon, const uint8 Variant, const bool bFullCell)
						{
							auto& Batch = FindOrAddBatch(Mesh, EVoxelRenderGroup::Water, Face.Bank, Variant);
							if (bFullCell) AppendQuad(Batch.Mesh, Polygon[0], Polygon[1], Polygon[2], Polygon[3], Face);
							else AppendWaterPolygon(Batch.Mesh, Polygon, Face);
						});
				}
			}
		}
	}

	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		if (Batch.Mesh.Validate()) continue;
		OutError =
			TEXT("Continuous water mesh validation failed");

		return false;
	}

	const double TexturePeriod = VoxelViewLod::TexturePeriodCells(InStep, InMaximumTextureStretchCells);
	for (FVoxelRenderBatch& OutputBatch : Mesh.Batches)
	{
		for (FVector2D& UV : OutputBatch.Mesh.UV0)
		{
			UV /= TexturePeriod;
		}
	}

	if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid water mesh"); return false; }
	OutMesh =
		MoveTemp(Mesh);

	OutError.Reset();
	return true;
}
