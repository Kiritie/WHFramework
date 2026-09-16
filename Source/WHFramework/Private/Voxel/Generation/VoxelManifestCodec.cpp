#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
namespace
{
void Recipe(FVoxelByteWriter& W, const FVoxelWorldManifest& M)
{
	const auto& S = M.Settings;
	W.U32(FVoxelWorldManifest::GeneratorVersion);
	W.I32(M.BlockSizeCentimeters);
	W.I32(S.Seed);
	W.I32(S.MinZ);
	W.I32(S.MaxZ);
	W.I32(S.SeaLevel);
	W.I32(S.BaseHeight);
	W.I32(S.ContinentalPeriod);
	W.I32(S.ErosionPeriod);
	W.I32(S.MountainPeriod);
	W.I32(S.ClimatePeriod);
	W.I32(S.DetailPeriod);
	W.I32(S.ContinentalAmplitude);
	W.I32(S.MountainAmplitude);
	W.I32(S.DetailAmplitude);
}
bool ReadRecipe(FVoxelByteReader& R, FVoxelWorldManifest& M)
{
	if (R.U32() != FVoxelWorldManifest::GeneratorVersion)
		return false;
	M.BlockSizeCentimeters = R.I32();
	auto& S = M.Settings;
	S.Seed = R.I32();
	S.MinZ = R.I32();
	S.MaxZ = R.I32();
	S.SeaLevel = R.I32();
	S.BaseHeight = R.I32();
	S.ContinentalPeriod = R.I32();
	S.ErosionPeriod = R.I32();
	S.MountainPeriod = R.I32();
	S.ClimatePeriod = R.I32();
	S.DetailPeriod = R.I32();
	S.ContinentalAmplitude = R.I32();
	S.MountainAmplitude = R.I32();
	S.DetailAmplitude = R.I32();
	FString E;
	return R.IsValid() && M.BlockSizeCentimeters >= 1 && M.BlockSizeCentimeters <= 10000 && S.Validate(E);
}
}
uint64 FVoxelManifestCodec::RecipeFingerprint(const FVoxelWorldManifest& M)
{
	FVoxelByteWriter W(256);
	Recipe(W, M);
	TArray<uint8> B;
	const bool OK = W.Finish(B);
	check(OK);
	return VoxelBinary::Hash(B);
}
bool FVoxelManifestCodec::Encode(const FVoxelWorldManifest& M, TArray<uint8>& O)
{
	FString E;
	if (!M.WorldId.IsValid() || !M.Settings.Validate(E) || M.BlockSizeCentimeters < 1 || M.BlockSizeCentimeters > 10000)
		return false;
	FVoxelByteWriter W(512);
	W.U32(0x324d5856);
	W.Guid(M.WorldId);
	Recipe(W, M);
	W.U64(M.RegistryHash);
	W.U64(RecipeFingerprint(M));
	W.U64(M.BaseSampleHash);
	return W.Finish(O);
}
bool FVoxelManifestCodec::Decode(TConstArrayView<uint8> B, FVoxelWorldManifest& O)
{
	if (B.Num() > 512)
		return false;
	FVoxelByteReader R(B);
	if (R.U32() != 0x324d5856)
		return false;
	FVoxelWorldManifest T;
	T.WorldId = R.Guid();
	if (!T.WorldId.IsValid() || !ReadRecipe(R, T))
		return false;
	T.RegistryHash = R.U64();
	T.RecipeHash = R.U64();
	T.BaseSampleHash = R.U64();
	if (!R.End() || T.RecipeHash != RecipeFingerprint(T))
		return false;
	O = T;
	return true;
}
