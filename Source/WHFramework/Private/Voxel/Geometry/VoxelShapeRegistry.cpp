#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
namespace
{
FVector2D MakeFaceUV(uint8 Face, const FVector& Position)
{
	if (Face < 2)
	{
		return FVector2D(Position.Y, 1.0 - Position.Z);
	}
	if (Face < 4)
	{
		return FVector2D(Position.X, 1.0 - Position.Z);
	}
	return FVector2D(Position.X, Position.Y);
}

FVector RotatePoint(FVector P, uint8 Y)
{
	switch (Y & 3)
	{
		case 1:
			return FVector(1 - P.Y, P.X, P.Z);
		case 2:
			return FVector(1 - P.X, 1 - P.Y, P.Z);
		case 3:
			return FVector(P.Y, 1 - P.X, P.Z);
		default:
			return P;
	}
}
FBox RotateBox(const FBox& B, uint8 Y)
{
	FBox O(ForceInit);
	for (int32 I = 0; I < 8; ++I)
		O += RotatePoint(FVector(I & 1 ? B.Max.X : B.Min.X, I & 2 ? B.Max.Y : B.Min.Y, I & 4 ? B.Max.Z : B.Min.Z), Y);
	return O;
}
bool Inside(const TArray<FBox>& B, const FVector& P)
{
	for (const FBox& X : B)
		if (X.IsInsideOrOn(P))
			return true;
	return false;
}
FVoxelShapeQuad Quad(uint8 Face, double Plane, double U0, double U1, double V0, double V1)
{
	int32 A = Face / 2, U = (A + 1) % 3, V = (A + 2) % 3;
	FVoxelShapeQuad Q;
	Q.Face = Face;
	Q.MaterialFace = Face;
	Q.bBoundary = FMath::IsNearlyEqual(Plane, Face % 2 == 0 ? 1.0 : 0.0, 1e-8);
	const double Us[4] = {U0, U1, U1, U0}, Vs[4] = {V0, V0, V1, V1};
	for (int32 I = 0; I < 4; ++I)
	{
		Q.Vertices[I] = FVector::ZeroVector;
		Q.Vertices[I][A] = Plane;
		Q.Vertices[I][U] = Us[I];
		Q.Vertices[I][V] = Vs[I];
		Q.UV[I] = MakeFaceUV(Face, Q.Vertices[I]);
	}
	if (Face & 1)
	{
		Swap(Q.Vertices[1], Q.Vertices[3]);
		Swap(Q.UV[1], Q.UV[3]);
	}
	return Q;
}
void UnionQuads(const TArray<FBox>& Boxes, TArray<FVoxelShapeQuad>& Out)
{
	TArray<double> C[3];
	for (int32 A = 0; A < 3; ++A)
	{
		C[A] = {0, 1};
		for (const auto& B : Boxes)
		{
			C[A].AddUnique(B.Min[A]);
			C[A].AddUnique(B.Max[A]);
		}
		C[A].Sort();
	}
	for (int32 Z = 0; Z < C[2].Num() - 1; ++Z)
		for (int32 Y = 0; Y < C[1].Num() - 1; ++Y)
			for (int32 X = 0; X < C[0].Num() - 1; ++X)
			{
				const int32 I[3] = {X, Y, Z};
				FVector Mid((C[0][X] + C[0][X + 1]) / 2, (C[1][Y] + C[1][Y + 1]) / 2, (C[2][Z] + C[2][Z + 1]) / 2);
				if (!Inside(Boxes, Mid))
					continue;
				for (uint8 F = 0; F < 6; ++F)
				{
					int32 A = F / 2, U = (A + 1) % 3, V = (A + 2) % 3;
					double P = C[A][I[A] + (F % 2 == 0 ? 1 : 0)];
					FVector Test = Mid;
					Test[A] = P + (F % 2 == 0 ? 1e-6 : -1e-6);
					if (!Inside(Boxes, Test))
						Out.Add(Quad(F, P, C[U][I[U]], C[U][I[U] + 1], C[V][I[V]], C[V][I[V] + 1]));
				}
			}
}
void FillCoverage(FVoxelResolvedShape& S)
{
	for (uint8 F = 0; F < 6; ++F)
	{
		int32 A = F / 2, U = (A + 1) % 3, V = (A + 2) % 3;
		int32 Covered = 0;
		for (int32 Y = 0; Y < 16; ++Y)
			for (int32 X = 0; X < 16; ++X)
			{
				FVector P(0, 0, 0);
				P[A] = F % 2 == 0 ? 1 - 1e-6 : 1e-6;
				P[U] = (X + .5) / 16;
				P[V] = (Y + .5) / 16;
				if (Inside(S.SelectionBoxes, P))
				{
					int32 I = X + Y * 16;
					S.Coverage[F][I / 64] |= uint64(1) << (I % 64);
					++Covered;
				}
			}
		if (Covered == 256)
			S.OcclusionMask |= uint8(1u << F);
	}
}
}
bool FVoxelShapeRegistry::IsValidState(EVoxelShapeKind K, uint16 S)
{
	const uint16 F = S & 7;
	uint16 Allowed = 0;
	switch (K)
	{
		case EVoxelShapeKind::FullCube:
			Allowed = 7;
			if (F > 3)
				return false;
			break;
		case EVoxelShapeKind::Slab:
			Allowed = 8 | 0x800;
			if ((S & 0x800) && (S & 8))
				return false;
			break;
		case EVoxelShapeKind::Stair:
			Allowed = 7 | 8;
			if (F > 3)
				return false;
			break;
		case EVoxelShapeKind::Ladder:
			Allowed = 7;
			if (F > 3)
				return false;
			break;
		case EVoxelShapeKind::Pane:
			Allowed = uint16(15u << 11);
			break;
		case EVoxelShapeKind::Door:
			Allowed = 7 | 8 | 16 | 32;
			if (F > 3)
				return false;
			break;
		case EVoxelShapeKind::Trapdoor:
			Allowed = 7 | 8 | 16;
			if (F > 3)
				return false;
			break;
		case EVoxelShapeKind::Torch:
			Allowed = 7;
			if (F > 4)
				return false;
			break;
		case EVoxelShapeKind::CrossPlant:
		case EVoxelShapeKind::Fluid:
			Allowed = 0;
			break;
		default:
			return false;
	}
	return (S & ~Allowed) == 0;
}
uint8 FVoxelShapeRegistry::RotateFace(uint8 F, uint8 Y)
{
	check(F < 6);
	if (F >= 4)
		return F;
	FIntVector D = VoxelCoord::Direction(F);
	for (uint8 I = 0; I < (Y & 3); ++I)
	{
		int32 X = D.X;
		D.X = -D.Y;
		D.Y = X;
	}
	return D.X == 1 ? 0 : D.X == -1 ? 1 : D.Y == 1 ? 2 : 3;
}
FVoxelResolvedShape FVoxelShapeRegistry::Build(EVoxelShapeKind K, uint16 S)
{
	FVoxelResolvedShape O;
	TArray<FBox> B;
	const double T = 1.0 / 16;
	uint8 Y = uint8(S & 7);
	auto Box = [&](double X0, double Y0, double Z0, double X1, double Y1, double Z1)
	{
		B.Add(FBox(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1)));
	};
	switch (K)
	{
		case EVoxelShapeKind::FullCube:
			Box(0, 0, 0, 1, 1, 1);
			break;
		case EVoxelShapeKind::Slab:
			Y = 0;
			if (S & 0x800)
				Box(0, 0, 0, 1, 1, 1);
			else if (S & 8)
				Box(0, 0, .5, 1, 1, 1);
			else
				Box(0, 0, 0, 1, 1, .5);
			break;
		case EVoxelShapeKind::Stair:
			if (S & 8)
			{
				Box(0, 0, .5, 1, 1, 1);
				Box(.5, 0, 0, 1, 1, .5);
			}
			else
			{
				Box(0, 0, 0, 1, 1, .5);
				Box(.5, 0, .5, 1, 1, 1);
			}
			break;
		case EVoxelShapeKind::Ladder:
			Box(0, 0, 0, T, 1, 1);
			break;
		case EVoxelShapeKind::Pane:
		{
			Y = 0;
			Box(7 * T, 7 * T, 0, 9 * T, 9 * T, 1);
			uint8 C = VoxelState::GetConnections(S);
			if (C & 1)
				Box(9 * T, 7 * T, 0, 1, 9 * T, 1);
			if (C & 2)
				Box(0, 7 * T, 0, 7 * T, 9 * T, 1);
			if (C & 4)
				Box(7 * T, 9 * T, 0, 9 * T, 1, 1);
			if (C & 8)
				Box(7 * T, 0, 0, 9 * T, 7 * T, 1);
			break;
		}
		case EVoxelShapeKind::Door:
			if (!(S & 16))
				Box(0, 0, 0, 3 * T, 1, 1);
			else if (S & 32)
				Box(0, 13 * T, 0, 1, 1, 1);
			else
				Box(0, 0, 0, 1, 3 * T, 1);
			break;
		case EVoxelShapeKind::Trapdoor:
			if (S & 16)
				Box(0, 0, 0, 3 * T, 1, 1);
			else if (S & 8)
				Box(0, 0, 13 * T, 1, 1, 1);
			else
				Box(0, 0, 0, 1, 1, 3 * T);
			break;
		case EVoxelShapeKind::Torch:
			if (Y == 4)
			{
				Y = 0;
				Box(7 * T, 7 * T, 0, 9 * T, 9 * T, 10 * T);
			}
			else
				Box(0, 7 * T, 6 * T, 8 * T, 9 * T, 9 * T);
			break;
		case EVoxelShapeKind::Fluid:
			Y = 0;
			Box(0, 0, 0, 1, 1, 1);
			break;
		case EVoxelShapeKind::CrossPlant:
		{
			for (int32 I = 0; I < 2; ++I)
			{
				FVoxelShapeQuad Q;
				Q.Face = 0;
				Q.MaterialFace = 0;
				Q.bTwoSided = true;
				Q.Vertices[0] = FVector(0, I ? 1 : 0, 0);
				Q.Vertices[1] = FVector(1, I ? 0 : 1, 0);
				Q.Vertices[2] = FVector(1, I ? 0 : 1, 1);
				Q.Vertices[3] = FVector(0, I ? 1 : 0, 1);
				Q.UV[0] = {0, 1};
				Q.UV[1] = {1, 1};
				Q.UV[2] = {1, 0};
				Q.UV[3] = {0, 0};
				O.Quads.Add(Q);
				O.SelectionQuads.Add(Q);
			}
			return O;
		}
		default:
			checkNoEntry();
			return O;
	}
	UnionQuads(B, O.Quads);
	for (auto& Q : O.Quads)
	{
		for (auto& P : Q.Vertices)
			P = RotatePoint(P, Y);
		Q.Face = RotateFace(Q.Face, Y);
	}
	for (auto& X : B)
		X = RotateBox(X, Y);
	O.SelectionBoxes = B;
	O.CollisionBoxes = B;
	if (K == EVoxelShapeKind::Ladder || K == EVoxelShapeKind::Torch || K == EVoxelShapeKind::Fluid)
		O.CollisionBoxes.Reset();
	FillCoverage(O);
	return O;
}
void FVoxelShapeRegistry::BuildDefaults()
{
	Templates.Reset();
	for (uint8 K = 0; K <= uint8(EVoxelShapeKind::Fluid); ++K)
		for (uint32 S = 0; S < 65536; ++S)
			if (IsValidState(EVoxelShapeKind(K), uint16(S)))
				Templates.Add(Key(EVoxelShapeKind(K), uint16(S)), Build(EVoxelShapeKind(K), uint16(S)));
}
const FVoxelResolvedShape* FVoxelShapeRegistry::Find(EVoxelShapeKind K, uint16 S) const
{
	return Templates.Find(Key(K, S));
}
const FVoxelResolvedShape& FVoxelShapeRegistry::Get(EVoxelShapeKind K, uint16 S) const
{
	const auto* V = Find(K, S);
	check(V);
	return *V;
}
