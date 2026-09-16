#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Containers/StringConv.h"
#include "Misc/Compression.h"
uint64 FVoxelDeltaCodec::MaxEncodedBytes(const FVoxelRegistrySnapshot& R, const FVoxelSectionOverlay& O)
{
	if (O.Blocks.Num() > 4096 || O.Entities.Num() > 256)
		return 0;
	uint64 Raw = 54 + uint64(O.Blocks.Num()) * 6, EntityBytes = 0;
	TSet<uint16> Types;
	for (const auto& P : O.Blocks)
	{
		if (P.Key >= 4096 || !R.IsValid(P.Value))
			return 0;
		if (!Types.Contains(P.Value.TypeId))
		{
			Types.Add(P.Value.TypeId);
			const auto* D = R.Find(P.Value.TypeId);
			FTCHARToUTF8 Name(*D->BlockName.ToString());
			if (Name.Length() <= 0 || Name.Length() > 256)
				return 0;
			Raw += 4 + uint64(Name.Length());
		}
	}
	for (const auto& P : O.Entities)
	{
		if (P.Key >= 4096 || !FVoxelBlockEntityCodec::Validate(P.Value))
			return 0;
		EntityBytes += uint64(P.Value.Payload.Num());
		Raw += 10 + uint64(P.Value.Payload.Num());
	}
	if (EntityBytes > 256 * 1024 || Raw > 1024 * 1024)
		return 0;
	const int32 Bound = FCompression::CompressMemoryBound(NAME_Zlib, int32(Raw));
	if (Bound <= 0)
		return 0;
	const uint64 Result = 16 + uint64(Bound);
	return Result <= MaxSectionWireBytes ? Result : 0;
}
bool FVoxelDeltaCodec::Encode(const FVoxelWorldManifest& M, const FVoxelRegistrySnapshot& R, const FVoxelSectionOverlay& O, TArray<uint8>& Out)
{
	if (!MaxEncodedBytes(R, O))
		return false;
	FVoxelByteWriter W(1024 * 1024);
	W.U32(0x32445856);
	W.Guid(M.WorldId);
	W.U64(M.RecipeHash);
	W.I32(O.Key.X);
	W.I32(O.Key.Y);
	W.I32(O.Key.Z);
	W.U64(O.Revision);
	TArray<uint16> Keys;
	O.Blocks.GetKeys(Keys);
	Keys.Sort();
	TArray<uint16> Types;
	for (uint16 I : Keys)
	{
		const auto& S = O.Blocks.FindChecked(I);
		if (I >= 4096 || !R.IsValid(S))
			return false;
		Types.AddUnique(S.TypeId);
	}
	Types.Sort(
	    [&](uint16 A, uint16 B)
	    {
		    return R.Find(A)->BlockName.ToString().Compare(R.Find(B)->BlockName.ToString(), ESearchCase::CaseSensitive) < 0;
	    });
	W.U16(uint16(Types.Num()));
	for (uint16 T : Types)
		W.String(R.Find(T)->BlockName.ToString(), 256);
	W.U16(uint16(Keys.Num()));
	for (uint16 I : Keys)
	{
		const auto& S = O.Blocks.FindChecked(I);
		W.U16(I);
		W.U16(uint16(Types.IndexOfByKey(S.TypeId)));
		W.U16(S.State);
	}
	O.Entities.GetKeys(Keys);
	Keys.Sort();
	W.U16(uint16(Keys.Num()));
	for (uint16 I : Keys)
	{
		const auto& E = O.Entities.FindChecked(I);
		if (I >= 4096 || !FVoxelBlockEntityCodec::Validate(E))
			return false;
		W.U16(I);
		W.U16(E.Kind);
		W.U16(E.Schema);
		W.Blob(E.Payload, 32768);
	}
	TArray<uint8> Raw;
	if (!W.Finish(Raw))
		return false;
	return VoxelBinary::Compress(Raw, Out, 1024 * 1024);
}
bool FVoxelDeltaCodec::Decode(TConstArrayView<uint8> B, const FVoxelWorldManifest& M, const FVoxelRegistrySnapshot& R, FVoxelSectionOverlay& Out)
{
	TArray<uint8> Raw;
	if (!VoxelBinary::Decompress(B, Raw, 1024 * 1024))
		return false;
	FVoxelByteReader Q(Raw);
	if (Q.U32() != 0x32445856 || Q.Guid() != M.WorldId || Q.U64() != M.RecipeHash)
		return false;
	FVoxelSectionOverlay O;
	O.Key.X = Q.I32();
	O.Key.Y = Q.I32();
	O.Key.Z = Q.I32();
	O.Revision = Q.U64();
	if (!VoxelCoord::IsValidSection(O.Key, M.Settings.MinZ, M.Settings.MaxZ))
		return false;
	uint16 N = Q.U16();
	if (N > 4096)
		return false;
	TArray<uint16> P;
	TSet<FName> Names;
	for (uint16 I = 0; I < N; ++I)
	{
		FName Name(*Q.String(256));
		const auto* D = R.Find(Name);
		if (!Q.IsValid() || !D || Names.Contains(Name))
			return false;
		Names.Add(Name);
		P.Add(D->TypeId);
	}
	uint16 C = Q.U16();
	if (C > 4096)
		return false;
	for (uint16 I = 0; I < C; ++I)
	{
		const uint16 L = Q.U16();
		const uint16 PaletteIndex = Q.U16();
		const uint16 StateValue = Q.U16();
		if (!Q.IsValid() || L >= 4096 || PaletteIndex >= P.Num() || O.Blocks.Contains(L))
			return false;
		FVoxelBlockState V{P[PaletteIndex], StateValue};
		if (!R.IsValid(V))
			return false;
		O.Blocks.Add(L, V);
	}
	uint16 EC = Q.U16();
	if (EC > 256)
		return false;
	uint64 Sum = 0;
	for (uint16 I = 0; I < EC; ++I)
	{
		uint16 L = Q.U16();
		FVoxelBlockEntityState E;
		E.Kind = Q.U16();
		E.Schema = Q.U16();
		E.Payload = Q.Blob(32768);
		Sum += E.Payload.Num();
		if (!Q.IsValid() || L >= 4096 || O.Entities.Contains(L) || Sum > 256 * 1024 || !FVoxelBlockEntityCodec::Validate(E))
			return false;
		O.Entities.Add(L, MoveTemp(E));
	}
	if (!Q.End() || !MaxEncodedBytes(R, O))
		return false;
	Out = MoveTemp(O);
	return true;
}
