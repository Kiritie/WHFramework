#include "Voxel/Rendering/VoxelHeightfieldMesher.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
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

	const FVector2D UVs[] =
	{
		FVector2D(0.0, 0.0),
		FVector2D(1.0, 0.0),
		FVector2D(1.0, 1.0),
		FVector2D(0.0, 1.0)
	};

	for (int32 Index = 0;
		Index < 4;
		++Index)
	{
		InOutMesh.Vertices.Add(
			Vertices[Index]);

		InOutMesh.Normals.Add(
			Normal);

		InOutMesh.UV0.Add(
			UVs[Index]);

		InOutMesh.UV1.Add(
			FVector2D(
				InFace.Layer,
				InFace.Frames));

		InOutMesh.UV2.Add(
			FVector2D(
				InFace.FPS,
				0.0));

		InOutMesh.Colors.Add(
			InColor);

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
	const int32 InSkirtDepthCells)
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
	const TAtomic<bool>* InCancel)
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

			const int32 I10 =
				VertexIndex(
					X + 1,
					Y,
					InWater.Side);

			const int32 I11 =
				VertexIndex(
					X + 1,
					Y + 1,
					InWater.Side);

			const int32 I01 =
				VertexIndex(
					X,
					Y + 1,
					InWater.Side);

			if (!IsWet(I00) ||
				!IsWet(I10) ||
				!IsWet(I11) ||
				!IsWet(I01))
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
					InWater.WaterZ[I10] +
						1.0),
				FVector(
					X1,
					Y1,
					InWater.WaterZ[I11] +
						1.0),
				FVector(
					X0,
					Y1,
					InWater.WaterZ[I01] +
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

	OutMesh =
		MoveTemp(Mesh);

	OutError.Reset();
	return true;
}
