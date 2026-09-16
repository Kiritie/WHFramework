#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
bool FVoxelCollisionBuilder::Build(
    const FVoxelSectionSnapshot& S, const FVoxelRegistrySnapshot& R, const FVoxelShapeRegistry& H, FVoxelSectionCollisionResult& O, const std::atomic_bool* C)
{
	if (S.Blocks.Num() != 4096)
		return false;
	FVoxelSectionCollisionResult T;
	T.Stamp = S.Stamp;
	uint8 Filled[4096] = {};
	for (uint16 I = 0; I < 4096; ++I)
	{
		auto B = FVoxelBlockState::Unpack(S.Blocks[I]);
		if (!R.IsValid(B))
			return false;
		if (B.IsAir())
			continue;
		const auto* D = R.Find(B.TypeId);
		if (!D->bSolid)
			continue;
		if (D->Shape == EVoxelShapeKind::FullCube)
			Filled[I] = 1;
		else
		{
			const auto* Sh = H.Find(D->Shape, B.State);
			if (!Sh)
				return false;
			FVector P(VoxelCoord::Unlinear(I));
			for (const auto& X : Sh->CollisionBoxes)
				T.Boxes.Add(FBox(X.Min + P, X.Max + P));
		}
	}
	auto At = [&](int32 X, int32 Y, int32 Z) -> uint8&
	{
		return Filled[X + 16 * Y + 256 * Z];
	};
	for (int32 Z = 0; Z < 16; ++Z)
		for (int32 Y = 0; Y < 16; ++Y)
			for (int32 X = 0; X < 16; ++X)
			{
				if (C && C->load(std::memory_order_relaxed))
					return false;
				if (!At(X, Y, Z))
					continue;
				int32 W = 1;
				int32 Ht = 1;
				int32 Dp = 1;
				while (X + W < 16 && At(X + W, Y, Z))
					++W;
				bool Stop = false;
				while (Y + Ht < 16 && !Stop)
				{
					for (int32 DX = 0; DX < W; ++DX)
						if (!At(X + DX, Y + Ht, Z))
						{
							Stop = true;
							break;
						}
					if (!Stop)
						++Ht;
				}
				Stop = false;
				while (Z + Dp < 16 && !Stop)
				{
					for (int32 DY = 0; DY < Ht && !Stop; ++DY)
						for (int32 DX = 0; DX < W; ++DX)
							if (!At(X + DX, Y + DY, Z + Dp))
							{
								Stop = true;
								break;
							}
					if (!Stop)
						++Dp;
				}
				for (int32 DZ = 0; DZ < Dp; ++DZ)
					for (int32 DY = 0; DY < Ht; ++DY)
						for (int32 DX = 0; DX < W; ++DX)
							At(X + DX, Y + DY, Z + DZ) = 0;
				T.Boxes.Add(FBox(FVector(X, Y, Z), FVector(X + W, Y + Ht, Z + Dp)));
			}
	if (T.Boxes.Num() > 16384)
		return false;
	O = MoveTemp(T);
	return true;
}
