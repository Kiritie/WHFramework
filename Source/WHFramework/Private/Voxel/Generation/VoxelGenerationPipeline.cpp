#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelSurfaceStage.h"
#include "Voxel/Chunks/VoxelSectionCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
bool FVoxelGenerationPipeline::GenerateSection(const FVoxelSectionKey& K, FVoxelSectionStorage& O, const std::atomic_bool* C) const
{
	if (!VoxelCoord::IsValidSection(K, Config.Settings.MinZ, Config.Settings.MaxZ))
		return false;
	TArray<uint32> D;
	D.SetNumUninitialized(4096);
	const FIntVector B = VoxelCoord::Origin(K);
	for (int32 Y = 0; Y < 16; ++Y)
		for (int32 X = 0; X < 16; ++X)
		{
			if (C && C->load(std::memory_order_relaxed))
				return false;
			const auto Column = Terrain.SampleColumn(B.X + X, B.Y + Y);
			for (int32 Z = 0; Z < 16; ++Z)
			{
				const int32 WZ = B.Z + Z;
				D[X + 16 * Y + 256 * Z] = FVoxelSurfaceStage::Resolve(Config, Column, WZ, Terrain.DensityQ16(B.X + X, B.Y + Y, WZ, Column.HeightQ16)).Pack();
			}
		}
	FVoxelSectionStorage T;
	if (!T.BuildFromDense(D))
		return false;
	O = MoveTemp(T);
	return true;
}
FVoxelBlockState FVoxelGenerationPipeline::SampleBaseBlock(const FIntVector& P) const
{
	if (!VoxelCoord::IsValid(P) || P.Z < Config.Settings.MinZ || P.Z >= Config.Settings.MaxZ)
		return {};
	const auto C = Terrain.SampleColumn(P.X, P.Y);
	return FVoxelSurfaceStage::Resolve(Config, C, P.Z, Terrain.DensityQ16(P.X, P.Y, P.Z, C.HeightQ16));
}
uint64 FVoxelGenerationPipeline::BuildHandshakeSignature() const
{
	const int32 SZ = VoxelCoord::FloorDiv(Config.Settings.BaseHeight, 16);
	const FVoxelSectionKey Keys[] = {{0, 0, SZ}, {-1, -1, SZ}, {37, -91, SZ}, {-307, 511, SZ}};
	FVoxelByteWriter W(64);
	for (const auto& K : Keys)
	{
		FVoxelSectionStorage S;
		const bool OK = GenerateSection(K, S);
		check(OK);
		W.U64(FVoxelSectionCodec::Fingerprint(S));
	}
	TArray<uint8> B;
	const bool OK = W.Finish(B);
	check(OK);
	return VoxelBinary::Hash(B);
}
