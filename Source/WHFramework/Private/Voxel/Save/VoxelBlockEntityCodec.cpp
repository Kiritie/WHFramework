#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
bool FVoxelBlockEntityCodec::EncodeContainer(const TArray<FVoxelItemStack>& S, FVoxelBlockEntityState& O)
{
	if (S.Num() != 27)
		return false;
	FVoxelByteWriter W(32768);
	W.U8(27);
	for (const auto& I : S)
	{
		if (I.Count < 0 || I.Count > 100000 || I.Level < 0 || I.Level > 10000 || ((I.Count > 0) != I.ID.IsValid()) || (I.Count == 0 && I.Level != 0))
			return false;
		W.String(I.ID.IsValid() ? I.ID.ToString() : FString(), 256);
		W.I32(I.Count);
		W.I32(I.Level);
	}
	FVoxelBlockEntityState T;
	T.Kind = 1;
	if (!W.Finish(T.Payload))
		return false;
	O = MoveTemp(T);
	return true;
}
bool FVoxelBlockEntityCodec::DecodeContainer(const FVoxelBlockEntityState& S, TArray<FVoxelItemStack>& O)
{
	if (S.Kind != 1 || S.Schema != 1 || S.Payload.Num() > 32768)
		return false;
	FVoxelByteReader R(S.Payload);
	if (R.U8() != 27)
		return false;
	TArray<FVoxelItemStack> T;
	for (int32 J = 0; J < 27; ++J)
	{
		FString Name = R.String(256);
		FVoxelItemStack I;
		I.ID = Name.IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId(Name);
		I.Count = R.I32();
		I.Level = R.I32();
		if (!R.IsValid() || I.Count < 0 || I.Count > 100000 || I.Level < 0 || I.Level > 10000 || ((I.Count > 0) != I.ID.IsValid()) ||
		    (I.Count == 0 && (!Name.IsEmpty() || I.Level != 0)))
			return false;
		T.Add(I);
	}
	if (!R.End())
		return false;
	O = MoveTemp(T);
	return true;
}
bool FVoxelBlockEntityCodec::MakeDefault(uint16 K, FVoxelBlockEntityState& O, uint8 Variant)
{
	if (K == 1)
	{
		TArray<FVoxelItemStack> S;
		S.SetNum(27);
		return EncodeContainer(S, O);
	}
	FVoxelBlockEntityState T;
	T.Kind = K;
	FVoxelByteWriter W(1024);
	if (K == 2)
		W.U64(0);
	else if (K == 100)
	{
		if (Variant < 1 || Variant > 2)
			return false;
		W.U8(Variant);
		W.U8(0);
		W.U16(0);
	} // spawner: kind, consumed, spawned GUID count
	else if (K == 101)
	{
		W.String(FString(), 256);
		W.U8(1);
	} // altar: team, enabled
	else
		return false;
	if (!W.Finish(T.Payload))
		return false;
	O = MoveTemp(T);
	return true;
}
bool FVoxelBlockEntityCodec::Validate(const FVoxelBlockEntityState& S)
{
	if (S.Schema != 1 || S.Payload.Num() > 32768)
		return false;
	if (S.Kind == 1)
	{
		TArray<FVoxelItemStack> T;
		return DecodeContainer(S, T);
	}
	FVoxelByteReader R(S.Payload);
	if (S.Kind == 2)
		R.U64();
	else if (S.Kind == 100)
	{
		uint8 K = R.U8(), Consumed = R.U8();
		uint16 N = R.U16();
		if (K < 1 || K > 2 || Consumed > 1 || N > 256)
			return false;
		TSet<FGuid> IDs;
		for (uint16 I = 0; I < N; ++I)
		{
			FGuid G = R.Guid();
			if (!G.IsValid() || IDs.Contains(G))
				return false;
			IDs.Add(G);
		}
	}
	else if (S.Kind == 101)
	{
		R.String(256);
		if (R.U8() > 1)
			return false;
	}
	else
		return false;
	return R.End();
}
bool FVoxelBlockEntityCodec::IncrementCounter(const FVoxelBlockEntityState& I, FVoxelBlockEntityState& O)
{
	if (I.Kind != 2 || !Validate(I))
		return false;
	FVoxelByteReader R(I.Payload);
	uint64 V = R.U64();
	if (V == MAX_uint64)
		return false;
	FVoxelByteWriter W(8);
	W.U64(V + 1);
	FVoxelBlockEntityState T;
	T.Kind = 2;
	if (!W.Finish(T.Payload))
		return false;
	O = MoveTemp(T);
	return true;
}
