#include "Voxel/Chunks/VoxelSectionKey.h"
int32 VoxelCoord::FloorDiv(int32 V, int32 D)
{
	check(D > 0);
	const int32 Q = V / D;
	return Q - (V % D < 0 ? 1 : 0);
}
bool VoxelCoord::IsValid(const FIntVector& P)
{
	const int64 M = VoxelBlock::MaxAbsCoordinate;
	return int64(P.X) >= -M && int64(P.X) < M && int64(P.Y) >= -M && int64(P.Y) < M && int64(P.Z) >= -M && int64(P.Z) < M;
}
FVoxelSectionKey VoxelCoord::Section(const FIntVector& P)
{
	return {FloorDiv(P.X, 16), FloorDiv(P.Y, 16), FloorDiv(P.Z, 16)};
}
FIntVector VoxelCoord::Origin(const FVoxelSectionKey& K)
{
	check(int64(K.X) * 16 >= MIN_int32 && int64(K.X) * 16 <= MAX_int32);
	check(int64(K.Y) * 16 >= MIN_int32 && int64(K.Y) * 16 <= MAX_int32);
	check(int64(K.Z) * 16 >= MIN_int32 && int64(K.Z) * 16 <= MAX_int32);
	return FIntVector(K.X * 16, K.Y * 16, K.Z * 16);
}
FIntVector VoxelCoord::Local(const FIntVector& P)
{
	return P - Origin(Section(P));
}
uint16 VoxelCoord::Linear(const FIntVector& L)
{
	check(L.X >= 0 && L.X < 16 && L.Y >= 0 && L.Y < 16 && L.Z >= 0 && L.Z < 16);
	return uint16(L.X + 16 * L.Y + 256 * L.Z);
}
FIntVector VoxelCoord::Unlinear(uint16 I)
{
	check(I < 4096);
	return FIntVector(I & 15, (I >> 4) & 15, (I >> 8) & 15);
}
FIntVector VoxelCoord::Direction(uint8 F)
{
	check(F < 6);
	FIntVector D(0, 0, 0);
	D[F / 2] = (F % 2 == 0 ? 1 : -1);
	return D;
}
FVoxelSectionKey VoxelCoord::Neighbor(const FVoxelSectionKey& K, uint8 F)
{
	const FIntVector D = Direction(F);
	return {K.X + D.X, K.Y + D.Y, K.Z + D.Z};
}
bool VoxelCoord::FromWorld(const FVector& P, double S, FIntVector& O)
{
	if (!FMath::IsFinite(S) || S <= 0 || P.ContainsNaN())
		return false;
	FVector G = P / S;
	const double M = VoxelBlock::MaxAbsCoordinate;
	if (!FMath::IsFinite(G.X) || !FMath::IsFinite(G.Y) || !FMath::IsFinite(G.Z) || G.X < -M || G.X >= M || G.Y < -M || G.Y >= M || G.Z < -M || G.Z >= M)
		return false;
	O = FIntVector(FMath::FloorToInt(G.X), FMath::FloorToInt(G.Y), FMath::FloorToInt(G.Z));
	return true;
}
FVector VoxelCoord::ToWorld(const FIntVector& P, double S)
{
	return FVector(double(P.X) * S, double(P.Y) * S, double(P.Z) * S);
}
bool VoxelCoord::IsValidSection(const FVoxelSectionKey& K, int32 MinZ, int32 MaxZ)
{
	const int64 X = int64(K.X) * 16, Y = int64(K.Y) * 16, Z = int64(K.Z) * 16, M = VoxelBlock::MaxAbsCoordinate;
	return MinZ < MaxZ && X >= -M && X + 15 < M && Y >= -M && Y + 15 < M && Z >= -M && Z + 15 < M && Z < MaxZ && Z + 16 > MinZ;
}
