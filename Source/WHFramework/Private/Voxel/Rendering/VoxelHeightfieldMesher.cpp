#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
	thread_local uint64* CountedMeshBytes = nullptr;
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

	void AppendWaterPolygon(FVoxelMeshBuffers& InOutMesh,
		TConstArrayView<FVector> InVertices, const FVoxelRuntimeFaceRef& InFace)
	{
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

	FVoxelRenderBatch& Batch =
		InOutMesh.Batches.
			AddDefaulted_GetRef();

	Batch.Group =
		InGroup;

	Batch.Bank =
		InBank;
	Batch.MaterialVariant = InVariant;

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
	TConstArrayView<uint16> InMaterials,
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

	auto AppendMaterialQuad =
		[&Mesh, &InConfig, &InRegistry, &InMaterials, &InHeights, &InTopTints, &CellIndex, &OutError, &Window, InVertexSide](
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
			const FLinearColor Tint = InFaceIndex == FacePositiveZ && !InTopTints.IsEmpty()
				? InTopTints[InTopTints.Num() == InVertexSide * InVertexSide ? CellIndex(InMaterialX, InMaterialY)
				: (FMath::Clamp(InMaterialX, Window.Min.X, Window.Max.X - 1) - Window.Min.X) + Window.Width() *
				(FMath::Clamp(InMaterialY, Window.Min.Y, Window.Max.Y - 1) - Window.Min.Y)] : FLinearColor::White;
			AppendQuad(Batch.Mesh, InA, InB, InC, InD, *Face, InColor * Tint);
			return true;
		};

	for (int32 Y = Window.Min.Y; Y < Window.Max.Y; ++Y)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}

		for (int32 X = Window.Min.X; X < Window.Max.X; ++X)
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

			if (X + 1 < CellSide && InHeights[CellIndex(X, Y)] != MIN_int32 &&
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

			if (Y + 1 < CellSide && InHeights[CellIndex(X, Y)] != MIN_int32 &&
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
		const double X0 = Cell.Min.X - InTileOrigin.X;
		const double X1 = Cell.Max.X - InTileOrigin.X;
		const double Y0 = Cell.Min.Y - InTileOrigin.Y;
		const double Y1 = Cell.Max.Y - InTileOrigin.Y;
		const double Z0 = Cell.Min.Z;
		const double Z1 = Cell.Max.Z;
		auto Face = [&](const uint8 FaceIndex, const FVector& A,
			const FVector& B, const FVector& C, const FVector& D)
		{
			const FVoxelRuntimeFaceRef& Material = Definition->Face(Cell.State.State, FaceIndex);
			FVoxelRenderBatch& Batch = FindOrAddBatch(InOutMesh,
				Definition->RenderGroup, Material.Bank);
			const int32 FirstUV = Batch.Mesh.UV0.Num();
			AppendQuad(Batch.Mesh, A, B, C, D, Material);
			for (int32 Index = FirstUV; Index < Batch.Mesh.UV0.Num(); ++Index)
			{
				Batch.Mesh.UV0[Index] /= TexturePeriod;
			}
		};
		Face(FacePositiveX, {X1, Y1, Z1}, {X1, Y0, Z1}, {X1, Y0, Z0}, {X1, Y1, Z0});
		Face(FaceNegativeX, {X0, Y0, Z1}, {X0, Y1, Z1}, {X0, Y1, Z0}, {X0, Y0, Z0});
		Face(FacePositiveY, {X0, Y1, Z1}, {X1, Y1, Z1}, {X1, Y1, Z0}, {X0, Y1, Z0});
		Face(FaceNegativeY, {X1, Y0, Z1}, {X0, Y0, Z1}, {X0, Y0, Z0}, {X1, Y0, Z0});
		Face(FacePositiveZ, {X0, Y0, Z1}, {X1, Y0, Z1}, {X1, Y1, Z1}, {X0, Y1, Z1});
		Face(FaceNegativeZ, {X0, Y1, Z0}, {X1, Y1, Z0}, {X1, Y0, Z0}, {X0, Y0, Z0});
	}
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
		InMacro.SurfaceClass.Num() != Count ||
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
		TopTints.Init(FLinearColor::White, Window.Width() * Window.Height());
		for (int32 Y = Window.Min.Y; Y < Window.Max.Y; ++Y)
		for (int32 X = Window.Min.X; X < Window.Max.X; ++X)
		{
			const int32 Index = X + Y * InMacro.Side;
			if (InMacro.SurfaceClass[Index] == InConfig.Recipe->Palette.Grass &&
				(InMacro.WaterHeight[Index] == MIN_int32 || InMacro.WaterHeight[Index] < InMacro.Height[Index]))
			{
				TopTints[(X - Window.Min.X) + Window.Width() * (Y - Window.Min.Y)] = FMath::Lerp(FLinearColor::White, LeafTint,
					static_cast<float>(InMacro.ForestCoverage[Index]) / 255.0f);
			}
		}
	}
	FVoxelSectionMeshResult Mesh;
	if (!BuildBlockyTerrain(
		InMacro.Side, InMacro.Step, InMacro.Height, InMacro.SurfaceClass,
		InConfig, InRegistry, Mesh, OutError, InCancel, {}, -0.05,
		InMaximumTextureStretchCells, TopTints, Window))
	{
		return false;
	}

	FVoxelSectionMeshResult WaterMesh;
	if (!BuildWaterGrid(InMacro.Side, InMacro.Step, InMacro.Height, InMacro.WaterHeight,
		InMacro.WaterKind, InRegistry, WaterMesh, OutError, InCancel, InMaximumTextureStretchCells, Window)) return false;
	for (auto& Batch : WaterMesh.Batches)
		if (!Batch.Mesh.Triangles.IsEmpty()) Mesh.Batches.Add(MoveTemp(Batch));
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



	auto IsWet =
		[&InWaterZ, &InWaterKind, &InGroundZ](
			const int32 InIndex)
		{
			return
				InWaterKind[InIndex] !=
					static_cast<uint8>(
						EVoxelWaterKind::None) &&
				InWaterZ[InIndex] !=
					MIN_int32 &&
				InWaterZ[InIndex] >=
					InGroundZ[InIndex];
		};

	for (int32 Y = Window.Min.Y;
		Y < Window.Max.Y;
		++Y)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 X = Window.Min.X;
			X < Window.Max.X;
			++X)
		{
			const int32 I00 =
				VertexIndex(
					X,
					Y,
					InSide);

			const int32 I10 = VertexIndex(X + 1, Y, InSide);
			const int32 I11 = VertexIndex(X + 1, Y + 1, InSide);
			const int32 I01 = VertexIndex(X, Y + 1, InSide);
			const bool bWet[4] = {IsWet(I00), IsWet(I10), IsWet(I11), IsWet(I01)};
			const uint8 WetMask = (bWet[0] ? 1 : 0) |
				(bWet[1] ? 2 : 0) | (bWet[2] ? 4 : 0) |
				(bWet[3] ? 8 : 0);
			if (WetMask == 0) continue;
			const int32 WetIndex = bWet[0] ? I00 : bWet[1] ? I10 : bWet[2] ? I11 : I01;
			FVoxelRenderBatch& Batch = FindOrAddBatch(Mesh, EVoxelRenderGroup::Water, Face.Bank, InWaterKind[WetIndex]);

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

			const FVector Corners[4] = {
				FVector(X0, Y0, bWet[0] ? InWaterZ[I00] + 1.0 : 0.0),
				FVector(X1, Y0, bWet[1] ? InWaterZ[I10] + 1.0 : 0.0),
				FVector(X1, Y1, bWet[2] ? InWaterZ[I11] + 1.0 : 0.0),
				FVector(X0, Y1, bWet[3] ? InWaterZ[I01] + 1.0 : 0.0)};
			if (WetMask == 15)
			{
				AppendQuad(Batch.Mesh, Corners[0], Corners[1],
					Corners[2], Corners[3], Face);
				continue;
			}
			auto Boundary = [&Corners](const int32 Wet, const int32 Dry)
			{
				const FVector& Water = Corners[Wet];
				const FVector& Land = Corners[Dry];
				return FVector((Water.X + Land.X) * 0.5,
					(Water.Y + Land.Y) * 0.5, Water.Z);
			};
			for (int32 Start = 0; Start < 4; ++Start)
			{
				if (!bWet[Start] || bWet[(Start + 3) & 3]) continue;
				TArray<FVector, TInlineAllocator<6>> Polygon;
				Polygon.Add(Boundary(Start, (Start + 3) & 3));
				int32 Corner = Start;
				for (int32 RunIndex = 0; RunIndex < 4 && bWet[Corner]; ++RunIndex)
				{
					Polygon.Add(Corners[Corner]);
					const int32 Next = (Corner + 1) & 3;
					if (!bWet[Next])
					{
						Polygon.Add(Boundary(Corner, Next));
						break;
					}
					Corner = Next;
				}
				AppendWaterPolygon(Batch.Mesh,
					MakeArrayView(Polygon.GetData(), Polygon.Num()), Face);
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
