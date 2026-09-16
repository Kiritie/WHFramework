#include "Voxel/Generation/VoxelNoiseRouter.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
int64 FVoxelNoiseRouter::LerpQ16(int64 A, int64 B, int32 T)
{
	return A + ((B - A) * int64(T)) / 65536;
}
int32 FVoxelNoiseRouter::Fraction(int32 V, int32 C, int32 P)
{
	int64 T = (int64(V) - int64(C) * P) * 65536 / P;
	return int32((T * T / 65536) * (196608 - 2 * T) / 65536);
}
int32 FVoxelNoiseRouter::Lattice(int32 X, int32 Y, int32 Z, uint32 Salt) const
{
	uint64 H = VoxelBinary::Mix64(uint32(Seed));
	H = VoxelBinary::Mix64(H ^ uint32(X));
	H = VoxelBinary::Mix64(H ^ uint32(Y));
	H = VoxelBinary::Mix64(H ^ uint32(Z));
	H = VoxelBinary::Mix64(H ^ Salt);
	return int32(H & 0xffff) - 32768;
}
int32 FVoxelNoiseRouter::Noise2D(int32 X, int32 Y, int32 P, uint32 S) const
{
	return Noise3D(X, Y, 0, P, S);
}
int32 FVoxelNoiseRouter::Noise3D(int32 X, int32 Y, int32 Z, int32 P, uint32 S) const
{
	check(P >= 2);
	const int32 A = VoxelCoord::FloorDiv(X, P), B = VoxelCoord::FloorDiv(Y, P), C = VoxelCoord::FloorDiv(Z, P);
	const int32 TX = Fraction(X, A, P), TY = Fraction(Y, B, P), TZ = Fraction(Z, C, P);
	int64 ZValues[2];
	for (int32 K = 0; K < 2; ++K)
	{
		int64 YValues[2];
		for (int32 J = 0; J < 2; ++J)
			YValues[J] = LerpQ16(Lattice(A, B + J, C + K, S), Lattice(A + 1, B + J, C + K, S), TX);
		ZValues[K] = LerpQ16(YValues[0], YValues[1], TY);
	}
	return int32(LerpQ16(ZValues[0], ZValues[1], TZ));
}
