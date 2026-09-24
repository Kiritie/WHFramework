#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
	constexpr uint8 FacePositiveX = 0;
	constexpr uint8 FaceNegativeX = 1;
	constexpr uint8 FacePositiveY = 2;
	constexpr uint8 FaceNegativeY = 3;
	constexpr uint8 FacePositiveZ = 4;
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
		const uint16 InSymbol,
		const uint8 InFaceIndex,
		const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelRuntimeDefinition*& OutDefinition,
		const FVoxelRuntimeFaceRef*& OutFace,
		FString& OutError)
	{
		FVoxelBlockState State;

		if (!InConfig.ToRuntime(InSymbol, State))
		{
			OutError = TEXT("Blocky heightfield contains an invalid material symbol");
			return false;
		}

		OutDefinition = InRegistry.Find(State.TypeId);

		if (!OutDefinition)
		{
			OutError = TEXT("Blocky heightfield material is missing from registry");
			return false;
		}

		OutFace = &OutDefinition->Face(State.State, InFaceIndex);
		return true;
	}
}

FVoxelRenderBatch& FVoxelHeightfieldMesher::FindOrAddBatch(
	FVoxelSectionMeshResult& InOutMesh,
	const EVoxelRenderGroup InGroup,
	const uint16 InBank)
{
	for (FVoxelRenderBatch& Batch :
		InOutMesh.Batches)
	{
		if (Batch.Group ==
				InGroup &&
			Batch.Bank ==
				InBank)
		{
			return Batch;
		}
	}

	FVoxelRenderBatch& Batch =
		InOutMesh.Batches.
			AddDefaulted_GetRef();

	Batch.Group =
		InGroup;

	Batch.Bank =
		InBank;

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

void FVoxelHeightfieldMesher::AppendSkirtQuad(
	FVoxelMeshBuffers& InOutMesh,
	const FVector& InTopA,
	const FVector& InTopB,
	const double InBottomZ,
	const FVoxelRuntimeFaceRef& InFace)
{
	const FVector BottomB(
		InTopB.X,
		InTopB.Y,
		FMath::Min(
			InTopB.Z,
			InBottomZ));

	const FVector BottomA(
		InTopA.X,
		InTopA.Y,
		FMath::Min(
			InTopA.Z,
			InBottomZ));

	AppendQuad(
		InOutMesh,
		InTopA,
		InTopB,
		BottomB,
		BottomA,
		InFace);
}

bool FVoxelHeightfieldMesher::BuildBlockyTerrain(
	const int32 InVertexSide,
	const int32 InStep,
	TConstArrayView<int32> InHeights,
	TConstArrayView<uint16> InMaterials,
	const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	TConstArrayView<uint8> InCoverage,
	const double InZBiasCells,
	const int32 InSkirtDepthCells,
	const double InMaximumTextureStretchCells)
{
	if (InVertexSide < 2 ||
		InStep <= 0 ||
		InHeights.Num() != InVertexSide * InVertexSide ||
		InMaterials.Num() != InHeights.Num() ||
		(!InCoverage.IsEmpty() && InCoverage.Num() != InHeights.Num()))
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

	auto AppendMaterialQuad =
		[&Mesh, &InConfig, &InRegistry, &InMaterials, &InHeights, &CellIndex, &OutError](
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
					InConfig,
					InRegistry,
					Definition,
					Face,
					OutError))
			{
				return false;
			}

			FVoxelRenderBatch& Batch = FindOrAddBatch(Mesh, Definition->RenderGroup, Face->Bank);
			AppendQuad(Batch.Mesh, InA, InB, InC, InD, *Face, InColor);
			return true;
		};

	for (int32 Y = 0; Y < CellSide; ++Y)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}

		for (int32 X = 0; X < CellSide; ++X)
		{
			const double X0 = static_cast<double>(X * InStep);
			const double X1 = static_cast<double>((X + 1) * InStep);
			const double Y0 = static_cast<double>(Y * InStep);
			const double Y1 = static_cast<double>((Y + 1) * InStep);
			const double CurrentTop = TopZ(X, Y);
			const FLinearColor Color = CoverageColor(X, Y);

			if (!AppendMaterialQuad(
					X,
					Y,
					FacePositiveZ,
					FVector(X0, Y0, CurrentTop),
					FVector(X1, Y0, CurrentTop),
					FVector(X1, Y1, CurrentTop),
					FVector(X0, Y1, CurrentTop),
					Color))
			{
				return false;
			}

			if (X + 1 < InVertexSide && InHeights[CellIndex(X, Y)] != MIN_int32 &&
				InHeights[CellIndex(X + 1, Y)] != MIN_int32)
			{
				const double NeighborTop = TopZ(X + 1, Y);

				if (CurrentTop > NeighborTop &&
					!AppendMaterialQuad(
						X, Y, FacePositiveX,
						FVector(X1, Y1, CurrentTop), FVector(X1, Y0, CurrentTop),
						FVector(X1, Y0, NeighborTop), FVector(X1, Y1, NeighborTop), Color))
				{
					return false;
				}

				if (NeighborTop > CurrentTop &&
					!AppendMaterialQuad(
						X + 1, Y, FaceNegativeX,
						FVector(X1, Y0, NeighborTop), FVector(X1, Y1, NeighborTop),
						FVector(X1, Y1, CurrentTop), FVector(X1, Y0, CurrentTop),
						CoverageColor(X + 1, Y)))
				{
					return false;
				}
			}

			if (Y + 1 < InVertexSide && InHeights[CellIndex(X, Y)] != MIN_int32 &&
				InHeights[CellIndex(X, Y + 1)] != MIN_int32)
			{
				const double NeighborTop = TopZ(X, Y + 1);

				if (CurrentTop > NeighborTop &&
					!AppendMaterialQuad(
						X, Y, FacePositiveY,
						FVector(X0, Y1, CurrentTop), FVector(X1, Y1, CurrentTop),
						FVector(X1, Y1, NeighborTop), FVector(X0, Y1, NeighborTop), Color))
				{
					return false;
				}

				if (NeighborTop > CurrentTop &&
					!AppendMaterialQuad(
						X, Y + 1, FaceNegativeY,
						FVector(X1, Y1, NeighborTop), FVector(X0, Y1, NeighborTop),
						FVector(X0, Y1, CurrentTop), FVector(X1, Y1, CurrentTop),
						CoverageColor(X, Y + 1)))
				{
					return false;
				}
			}
		}
	}

	if (InSkirtDepthCells > 0)
	{
		auto BuildEdgeSide =
			[&AppendMaterialQuad, &CoverageColor, InSkirtDepthCells](
				const int32 InX,
				const int32 InY,
				const uint8 InFace,
				const FVector& InTopA,
				const FVector& InTopB)
			{
				const double BottomZ = FMath::Min(InTopA.Z, InTopB.Z) - InSkirtDepthCells;
				FVector BottomB = InTopB;
				FVector BottomA = InTopA;
				BottomB.Z = BottomZ;
				BottomA.Z = BottomZ;
				return AppendMaterialQuad(
					InX, InY, InFace, InTopA, InTopB, BottomB, BottomA, CoverageColor(InX, InY));
			};

		for (int32 Index = 0; Index < CellSide; ++Index)
		{
			const double MinYTop = TopZ(Index, 0);
			const double MaxYTop = TopZ(Index, CellSide - 1);
			const double MinXTop = TopZ(0, Index);
			const double MaxXTop = TopZ(CellSide - 1, Index);

			if (!BuildEdgeSide(
					Index, 0, FaceNegativeY,
					FVector((Index + 1) * InStep, 0.0, MinYTop),
					FVector(Index * InStep, 0.0, MinYTop)) ||
				!BuildEdgeSide(
					Index, CellSide - 1, FacePositiveY,
					FVector(Index * InStep, CellSide * InStep, MaxYTop),
					FVector((Index + 1) * InStep, CellSide * InStep, MaxYTop)) ||
				!BuildEdgeSide(
					0, Index, FaceNegativeX,
					FVector(0.0, Index * InStep, MinXTop),
					FVector(0.0, (Index + 1) * InStep, MinXTop)) ||
				!BuildEdgeSide(
					CellSide - 1, Index, FacePositiveX,
					FVector(CellSide * InStep, (Index + 1) * InStep, MaxXTop),
					FVector(CellSide * InStep, Index * InStep, MaxXTop)))
			{
				return false;
			}
		}
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

	OutMesh = MoveTemp(Mesh);
	OutError.Reset();
	return true;
}

bool FVoxelHeightfieldMesher::BuildTerrain(
	const int32 InVertexSide,
	const int32 InStep,
	TConstArrayView<int32> InHeights,
	TConstArrayView<uint16> InMaterials,
	const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	TConstArrayView<uint8> InCoverage,
	const double InZBiasCells,
	const int32 InSkirtDepthCells,
	const double InMaximumTextureStretchCells)
{
	if (InVertexSide < 2 ||
		InStep <= 0 ||
		InHeights.Num() !=
			InVertexSide *
				InVertexSide ||
		InMaterials.Num() !=
			InHeights.Num() ||
		(!InCoverage.IsEmpty() &&
		 InCoverage.Num() !=
			InHeights.Num()))
	{
		OutError =
			TEXT("Invalid continuous heightfield input");

		return false;
	}

	FVoxelSectionMeshResult Mesh;

	const int32 CellSide =
		InVertexSide - 1;

	for (int32 Y = 0;
		Y < CellSide;
		++Y)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 X = 0;
			X < CellSide;
			++X)
		{
			const int32 I00 =
				VertexIndex(
					X,
					Y,
					InVertexSide);

			const int32 I10 =
				VertexIndex(
					X + 1,
					Y,
					InVertexSide);

			const int32 I11 =
				VertexIndex(
					X + 1,
					Y + 1,
					InVertexSide);

			const int32 I01 =
				VertexIndex(
					X,
					Y + 1,
					InVertexSide);

			FVoxelBlockState State;

			if (!InConfig.ToRuntime(
				InMaterials[I00],
				State))
			{
				OutError =
					TEXT("Heightfield contains an invalid material symbol");

				return false;
			}

			const FVoxelRuntimeDefinition* Definition =
				InRegistry.Find(
					State.TypeId);

			if (!Definition)
			{
				OutError =
					TEXT("Heightfield material is missing from the runtime registry");

				return false;
			}

			const FVoxelRuntimeFaceRef& Face =
				Definition->Face(
					State.State,
					TopMaterialFace);

			FVoxelRenderBatch& Batch =
				FindOrAddBatch(
					Mesh,
					Definition->
						RenderGroup,
					Face.Bank);

			const double X0 =
				X *
				InStep;

			const double X1 =
				(X + 1) *
				InStep;

			const double Y0 =
				Y *
				InStep;

			const double Y1 =
				(Y + 1) *
				InStep;

			const FVector A(
				X0,
				Y0,
				InHeights[I00] +
					1.0 +
					InZBiasCells);

			const FVector B(
				X1,
				Y0,
				InHeights[I10] +
					1.0 +
					InZBiasCells);

			const FVector C(
				X1,
				Y1,
				InHeights[I11] +
					1.0 +
					InZBiasCells);

			const FVector D(
				X0,
				Y1,
				InHeights[I01] +
					1.0 +
					InZBiasCells);

			const float Coverage =
				InCoverage.IsEmpty()
					? 1.0f
					: (
						InCoverage[I00] +
						InCoverage[I10] +
						InCoverage[I11] +
						InCoverage[I01]
					  ) /
					  (4.0f * 255.0f);

			AppendQuad(
				Batch.Mesh,
				A,
				B,
				C,
				D,
				Face,
				FLinearColor(
					Coverage,
					Coverage,
					Coverage,
					1.0f));
		}
	}

	if (InSkirtDepthCells > 0)
	{
		auto BuildSkirtSegment =
			[
				&InConfig,
				&InRegistry,
				&InMaterials,
				&InHeights,
				InVertexSide,
				InStep,
				InZBiasCells,
				InSkirtDepthCells,
				&Mesh,
				&OutError
			](
				const int32 InAX,
				const int32 InAY,
				const int32 InBX,
				const int32 InBY) -> bool
			{
				const int32 IA =
					VertexIndex(
						InAX,
						InAY,
						InVertexSide);

				const int32 IB =
					VertexIndex(
						InBX,
						InBY,
						InVertexSide);

				FVoxelBlockState State;

				if (!InConfig.ToRuntime(
					InMaterials[IA],
					State))
				{
					OutError =
						TEXT("Heightfield skirt material is invalid");

					return false;
				}

				const FVoxelRuntimeDefinition* Definition =
					InRegistry.Find(
						State.TypeId);

				if (!Definition)
				{
					OutError =
						TEXT("Heightfield skirt material is missing");

					return false;
				}

				const FVoxelRuntimeFaceRef& Face =
					Definition->Face(
						State.State,
						TopMaterialFace);

				FVoxelRenderBatch& Batch =
					FindOrAddBatch(
						Mesh,
						Definition->
							RenderGroup,
						Face.Bank);

				const FVector TopA(
					InAX *
						InStep,
					InAY *
						InStep,
					InHeights[IA] +
						1.0 +
						InZBiasCells);

				const FVector TopB(
					InBX *
						InStep,
					InBY *
						InStep,
					InHeights[IB] +
						1.0 +
						InZBiasCells);

				const double BottomZ =
					FMath::Min(
						TopA.Z,
						TopB.Z) -
					InSkirtDepthCells;

				AppendSkirtQuad(
					Batch.Mesh,
					TopA,
					TopB,
					BottomZ,
					Face);

				return true;
			};

		for (int32 Index = 0;
			Index < CellSide;
			++Index)
		{
			if (!BuildSkirtSegment(
					Index,
					0,
					Index + 1,
					0) ||
				!BuildSkirtSegment(
					Index + 1,
					CellSide,
					Index,
					CellSide) ||
				!BuildSkirtSegment(
					0,
					Index + 1,
					0,
					Index) ||
				!BuildSkirtSegment(
					CellSide,
					Index,
					CellSide,
					Index + 1))
			{
				return false;
			}
		}
	}

	for (const FVoxelRenderBatch& Batch :
		Mesh.Batches)
	{
		if (!Batch.Mesh.Validate())
		{
			OutError =
				TEXT("Continuous heightfield mesh validation failed");

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

	OutMesh =
		MoveTemp(Mesh);

	OutError.Reset();
	return true;
}

bool FVoxelHeightfieldMesher::BuildWater(
	const FVoxelWaterSurfaceTileData& InWater,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const double InMaximumTextureStretchCells)
{
	const int32 Count =
		InWater.Side *
		InWater.Side;

	if (InWater.Side < 2 ||
		InWater.Step <= 0 ||
		InWater.WaterZ.Num() !=
			Count ||
		InWater.GroundZ.Num() !=
			Count ||
		InWater.WaterKind.Num() !=
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

	FVoxelRenderBatch& Batch =
		FindOrAddBatch(
			Mesh,
			EVoxelRenderGroup::Water,
			Face.Bank);

	const int32 CellSide =
		InWater.Side - 1;

	auto IsWet =
		[&InWater](
			const int32 InIndex)
		{
			return
				InWater.WaterKind[InIndex] !=
					static_cast<uint8>(
						EVoxelWaterKind::None) &&
				InWater.WaterZ[InIndex] !=
					MIN_int32 &&
				InWater.WaterZ[InIndex] >=
					InWater.GroundZ[InIndex];
		};

	for (int32 Y = 0;
		Y < CellSide;
		++Y)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 X = 0;
			X < CellSide;
			++X)
		{
			const int32 I00 =
				VertexIndex(
					X,
					Y,
					InWater.Side);

			if (!IsWet(I00))
			{
				continue;
			}

			const double X0 =
				X *
				InWater.Step;

			const double X1 =
				(X + 1) *
				InWater.Step;

			const double Y0 =
				Y *
				InWater.Step;

			const double Y1 =
				(Y + 1) *
				InWater.Step;

			AppendQuad(
				Batch.Mesh,
				FVector(
					X0,
					Y0,
					InWater.WaterZ[I00] +
						1.0),
				FVector(
					X1,
					Y0,
					InWater.WaterZ[I00] +
						1.0),
				FVector(
					X1,
					Y1,
					InWater.WaterZ[I00] +
						1.0),
				FVector(
					X0,
					Y1,
					InWater.WaterZ[I00] +
						1.0),
				Face);
		}
	}

	if (!Batch.Mesh.Validate())
	{
		OutError =
			TEXT("Continuous water mesh validation failed");

		return false;
	}

	const double TexturePeriod = VoxelViewLod::TexturePeriodCells(InWater.Step, InMaximumTextureStretchCells);
	for (FVoxelRenderBatch& OutputBatch : Mesh.Batches)
	{
		for (FVector2D& UV : OutputBatch.Mesh.UV0)
		{
			UV /= TexturePeriod;
		}
	}

	OutMesh =
		MoveTemp(Mesh);

	OutError.Reset();
	return true;
}
