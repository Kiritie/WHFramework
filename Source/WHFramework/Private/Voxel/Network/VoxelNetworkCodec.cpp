#include "Voxel/Network/VoxelNetworkCodec.h"
bool FVoxelNetworkCodec::Encode(EVoxelMessage K, const FGuid& S, TConstArrayView<uint8> P, TArray<uint8>& O)
{
	FVoxelByteWriter W(2 * 1024 * 1024);
	W.U32(0x32544e56);
	W.U8(uint8(K));
	W.Guid(S);
	W.Blob(P, 2 * 1024 * 1024 - 64);
	return W.Finish(O);
}
bool FVoxelNetworkCodec::Decode(TConstArrayView<uint8> B, FVoxelWireMessage& O)
{
	if (B.Num() > 2 * 1024 * 1024)
		return false;
	FVoxelByteReader R(B);
	if (R.U32() != 0x32544e56)
		return false;
	uint8 K = R.U8();
	if (K < 1 || K > 14)
		return false;
	FVoxelWireMessage T;
	T.Kind = EVoxelMessage(K);
	T.Session = R.Guid();
	T.Payload = R.Blob(2 * 1024 * 1024 - 64);
	if (!R.End())
		return false;
	O = MoveTemp(T);
	return true;
}
void FVoxelNetworkCodec::WriteKey(FVoxelByteWriter& W, const FVoxelSectionKey& K)
{
	W.I32(K.X);
	W.I32(K.Y);
	W.I32(K.Z);
}
FVoxelSectionKey FVoxelNetworkCodec::ReadKey(FVoxelByteReader& R)
{
	FVoxelSectionKey K;
	K.X = R.I32();
	K.Y = R.I32();
	K.Z = R.I32();
	return K;
}
bool FVoxelNetworkCodec::EncodeIntent(const FVoxelEditIntent& I, TArray<uint8>& O)
{
	if (!I.RequestId || uint8(I.Action) > uint8(EVoxelEditAction::ContainerPut) || I.Origin.ContainsNaN() || I.Direction.ContainsNaN())
		return false;
	FVoxelByteWriter W(1024);
	W.U64(I.RequestId);
	W.U8(uint8(I.Action));
	for (int32 A = 0; A < 3; ++A)
	{
		if (FMath::Abs(I.Origin[A]) > 2e9)
			return false;
		W.I32(FMath::RoundToInt(I.Origin[A]));
	}
	FVector D = I.Direction.GetSafeNormal();
	if (D.IsNearlyZero())
		return false;
	for (int32 A = 0; A < 3; ++A)
		W.I32(FMath::RoundToInt(D[A] * 1000000));
	W.I32(I.ExpectedTarget.X);
	W.I32(I.ExpectedTarget.Y);
	W.I32(I.ExpectedTarget.Z);
	W.String(I.ExpectedItemID.IsValid() ? I.ExpectedItemID.ToString() : FString(), 256);
	W.I32(I.InventorySlot);
	W.I32(I.ContainerSlot);
	W.I32(I.Count);
	W.U64(I.ExpectedRevision);
	return W.Finish(O);
}
bool FVoxelNetworkCodec::DecodeIntent(TConstArrayView<uint8> B, FVoxelEditIntent& O)
{
	if (B.Num() > 1024)
		return false;
	FVoxelByteReader R(B);
	FVoxelEditIntent T;
	T.RequestId = R.U64();
	uint8 K = R.U8();
	if (!T.RequestId || K > uint8(EVoxelEditAction::ContainerPut))
		return false;
	T.Action = EVoxelEditAction(K);
	for (int32 A = 0; A < 3; ++A)
		T.Origin[A] = R.I32();
	for (int32 A = 0; A < 3; ++A)
		T.Direction[A] = double(R.I32()) / 1000000;
	T.ExpectedTarget.X = R.I32();
	T.ExpectedTarget.Y = R.I32();
	T.ExpectedTarget.Z = R.I32();
	FString ID = R.String(256);
	T.ExpectedItemID = ID.IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId(ID);
	T.InventorySlot = R.I32();
	T.ContainerSlot = R.I32();
	T.Count = R.I32();
	T.ExpectedRevision = R.U64();
	if (!R.End() || !VoxelCoord::IsValid(T.ExpectedTarget) || T.Direction.SizeSquared() < .99 || T.Direction.SizeSquared() > 1.01 ||
	    (!ID.IsEmpty() && !T.ExpectedItemID.IsValid()) || T.InventorySlot < 0 || T.InventorySlot > 255 || T.ContainerSlot < 0 || T.ContainerSlot >= 27 ||
	    T.Count < 1 || T.Count > 100000)
		return false;
	T.Direction.Normalize();
	O = MoveTemp(T);
	return true;
}
bool FVoxelNetworkCodec::EncodeSnapshots(const FVoxelSnapshotBatch& B, const FVoxelWorldManifest& M, const FVoxelRegistrySnapshot& R, TArray<uint8>& O)
{
	if (!B.Id.IsValid() || B.Sections.IsEmpty() || B.Sections.Num() > 32)
		return false;
	FVoxelByteWriter W(2 * 1024 * 1024 - 128);
	W.Guid(B.Id);
	W.U8(uint8(B.Sections.Num()));
	TSet<FVoxelSectionKey> Seen;
	uint64 Bound = 17;
	for (const auto& S : B.Sections)
	{
		if (Seen.Contains(S.Key))
			return false;
		Seen.Add(S.Key);
		TArray<uint8> Bytes;
		const uint64 Size = FVoxelDeltaCodec::MaxEncodedBytes(R, S);
		if (!Size)
			return false;
		Bound += 4 + Size;
		if (Bound > FVoxelDeltaCodec::MaxAtomicBatchWireBytes)
			return false;
		if (!FVoxelDeltaCodec::Encode(M, R, S, Bytes))
			return false;
		W.Blob(Bytes, 1024 * 1024);
	}
	return W.Finish(O);
}
bool FVoxelNetworkCodec::DecodeSnapshots(TConstArrayView<uint8> Bytes, const FVoxelWorldManifest& M, const FVoxelRegistrySnapshot& R, FVoxelSnapshotBatch& O)
{
	FVoxelByteReader Read(Bytes);
	FVoxelSnapshotBatch T;
	T.Id = Read.Guid();
	uint8 N = Read.U8();
	if (!T.Id.IsValid() || !N || N > 32)
		return false;
	TSet<FVoxelSectionKey> Seen;
	for (uint8 I = 0; I < N; ++I)
	{
		auto B = Read.Blob(1024 * 1024);
		FVoxelSectionOverlay S;
		FString Error;
		if (!Read.IsValid() || !FVoxelDeltaCodec::Decode(B, M, R, S) || Seen.Contains(S.Key))
			return false;
		Seen.Add(S.Key);
		T.Sections.Add(MoveTemp(S));
	}
	if (!Read.End())
		return false;
	O = MoveTemp(T);
	return true;
}
bool FVoxelNetworkCodec::EncodeReply(const FVoxelEditReply& I, TArray<uint8>& O)
{
	FVoxelByteWriter W(1024);
	W.U64(I.RequestId);
	W.U8(uint8(I.Code));
	W.String(I.Reason, 768);
	return W.Finish(O);
}
bool FVoxelNetworkCodec::DecodeReply(TConstArrayView<uint8> B, FVoxelEditReply& O)
{
	FVoxelByteReader R(B);
	FVoxelEditReply T;
	T.RequestId = R.U64();
	uint8 C = R.U8();
	if (C > uint8(EVoxelEditCode::InventoryFull))
		return false;
	T.Code = EVoxelEditCode(C);
	T.Reason = R.String(768);
	if (!R.End())
		return false;
	O = MoveTemp(T);
	return true;
}
