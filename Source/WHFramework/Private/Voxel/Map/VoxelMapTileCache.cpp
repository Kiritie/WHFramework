#include "Voxel/Map/VoxelMapTileCache.h"

#include "Engine/Texture2D.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

namespace
{
	struct FMapTilePixels : FVoxelTaskCustomPayload
	{
		TArray<FColor> Pixels;

		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + Pixels.GetAllocatedSize();
		}
	};


}

FVoxelMapTileCache::FVoxelMapTileCache(
	FVoxelTaskScheduler& InScheduler,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const uint64 InWorldEpoch,
	const double InBlockSize)
	: Scheduler(InScheduler)
	, Generator(MoveTemp(InGenerator))
	, Config(MoveTemp(InConfig))
	, WorldEpoch(InWorldEpoch)
	, BlockSize(InBlockSize)
{
}

int32 FVoxelMapTileCache::SelectStep(const float InCellPixelSize)
{
	for (const int32 Step : { 4, 8, 16, 32, 128 })
	{
		if (InCellPixelSize * Step >= 4.f)
		{
			return Step;
		}
	}
	return 512;
}

void FVoxelMapTileCache::VisibleTiles(
	const FSceneMapView& InView,
	const FVector2D& InPanelSize,
	const double InBlockSize,
	TArray<FVoxelMapTileKey>& OutKeys)
{
	OutKeys.Reset();
	if (!FMath::IsFinite(InView.Range) || InView.Range <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(InBlockSize) || InBlockSize <= 0.0 ||
		InPanelSize.X <= 0.0 || InPanelSize.Y <= 0.0)
	{
		return;
	}

	const double PixelsPerCell = InBlockSize * InPanelSize.X / InView.Range;
	const int32 Step = SelectStep(static_cast<float>(PixelsPerCell));
	const int64 TileCells = static_cast<int64>(TileSide) * Step;
	const double HalfX = InView.Range * 0.5 / InBlockSize;
	const double HalfY = HalfX * InPanelSize.Y / InPanelSize.X;
	const double Radius = FMath::Sqrt(HalfX * HalfX + HalfY * HalfY);
	const double CenterX = InView.Center.X / InBlockSize;
	const double CenterY = InView.Center.Y / InBlockSize;
	const int64 MinX = FMath::FloorToInt64((CenterX - Radius) / TileCells);
	const int64 MaxX = FMath::FloorToInt64((CenterX + Radius) / TileCells);
	const int64 MinY = FMath::FloorToInt64((CenterY - Radius) / TileCells);
	const int64 MaxY = FMath::FloorToInt64((CenterY + Radius) / TileCells);
	if (MaxX - MinX > 32 || MaxY - MinY > 32)
	{
		return;
	}
	for (int64 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int64 X = MinX; X <= MaxX; ++X)
		{
			const int64 OriginX = X * TileCells;
			const int64 OriginY = Y * TileCells;
			if (X >= MIN_int32 && X <= MAX_int32 &&
				Y >= MIN_int32 && Y <= MAX_int32 &&
				OriginX >= MIN_int32 && OriginY >= MIN_int32 &&
				OriginX + (TileSide - 1) * Step < MAX_int32 &&
				OriginY + (TileSide - 1) * Step < MAX_int32)
			{
				OutKeys.Add({ FIntPoint(static_cast<int32>(X), static_cast<int32>(Y)), Step });
			}
		}
	}
}

void FVoxelMapTileCache::Request(
	const TArray<FVoxelMapTileKey>& InKeys,
	const FSceneMapView& InView)
{
	check(IsInGameThread());
	if (!ColorResolver)
	{
		return;
	}
	for (const FVoxelMapTileKey& Key : InKeys)
	{
		if (FTile* Tile = Tiles.Find(Key))
		{
			Tile->LastUse = ++Sequence;
		}
	}

	TArray<FVoxelMapTileKey> Requests = InKeys;
	for (const FVoxelMapTileKey& Key : InKeys)
	{
		if (Key.Step < 512) Requests.AddUnique(ParentKey(Key, 512));
	}
	Requests.Sort([&](const FVoxelMapTileKey& A, const FVoxelMapTileKey& B)
	{
		const bool bATarget = InKeys.Contains(A);
		const bool bBTarget = InKeys.Contains(B);
		if (bATarget != bBTarget) return bATarget;
		const FVector2D Center = InView.Center / BlockSize;
		return FVector2D::DistSquared((FVector2D(A.Coordinate) + FVector2D(0.5)) * TileSide * A.Step, Center) <
			FVector2D::DistSquared((FVector2D(B.Coordinate) + FVector2D(0.5)) * TileSide * B.Step, Center);
	});
	int32 Admitted = 0;
	for (const FVoxelMapTileKey& Key : Requests)
	{
		if (Admitted >= 4 || Pending.Num() >= 8)
		{
			break;
		}
		if (Tiles.Contains(Key) || Pending.Contains(Key))
		{
			continue;
		}
		const int64 TileCells = static_cast<int64>(TileSide) * Key.Step;
		const FIntPoint Origin(
			static_cast<int32>(static_cast<int64>(Key.Coordinate.X) * TileCells),
			static_cast<int32>(static_cast<int64>(Key.Coordinate.Y) * TileCells));
		const TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> WorkerGenerator = Generator;
		const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> WorkerConfig = Config;
		FVoxelTaskRequest Task;
		Task.Kind = EVoxelTaskKind::BuildMapTile;
		Task.WorkClass = EVoxelWorkClass::Background;
		Task.Stamp.WorldEpoch = WorldEpoch;
		Task.Stamp.Token = ++Sequence;
		Task.ReservedBytes = 2ull * 1024ull * 1024ull;
		const FVector2D TileCenter(
			(static_cast<double>(Origin.X) + TileCells * 0.5),
			(static_cast<double>(Origin.Y) + TileCells * 0.5));
		Task.DistanceScore = FVector2D::Distance(TileCenter, InView.Center / BlockSize);
		const FVoxelMapColorResolver WorkerResolver = ColorResolver;
		const uint64 RequestStyleEpoch = StyleEpoch;
		Task.Execute = [Key, Origin, WorkerGenerator, WorkerConfig, WorkerResolver](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			Result.Kind = EVoxelTaskKind::BuildMapTile;
			TArray<FVoxelEnvironmentSample> Samples;
			if (!WorkerGenerator->SampleEnvironments(
				Origin, TileSide, TileSide, Key.Step, Samples,
				Result.Error, &Cancel, false))
			{
				return Result;
			}
			TSharedRef<FMapTilePixels, ESPMode::ThreadSafe> Pixels =
				MakeShared<FMapTilePixels, ESPMode::ThreadSafe>();
			Pixels->Pixels.SetNumUninitialized(TileSide * TileSide);
			for (int32 Y = 0; Y < TileSide; ++Y)
			{
				for (int32 X = 0; X < TileSide; ++X)
				{
					Pixels->Pixels[X + (TileSide - 1 - Y) * TileSide] =
						WorkerResolver(Samples[X + Y * TileSide].Column, *WorkerConfig);
				}
			}
			Result.CustomPayload = Pixels;
			Result.bSuccess = true;
			return Result;
		};
		Task.Apply = [this, Key, RequestStyleEpoch](FVoxelTaskResult&& Result)
		{
			if (StyleEpoch == RequestStyleEpoch)
			{
				ApplyTile(Key, MoveTemp(Result));
			}
		};
		if (Scheduler.Enqueue(MoveTemp(Task)))
		{
			Pending.Add(Key);
			++Admitted;
		}
	}
}

const FSlateBrush* FVoxelMapTileCache::FindBrush(const FVoxelMapTileKey& InKey) const
{
	const FTile* Tile = Tiles.Find(InKey);
	return Tile ? &Tile->Brush : nullptr;
}

void FVoxelMapTileCache::SetColorResolver(FVoxelMapColorResolver InResolver, const uint32 InStyleRevision)
{
	check(IsInGameThread());
	if (ColorResolver && StyleRevision == InStyleRevision)
	{
		return;
	}
	ColorResolver = MoveTemp(InResolver);
	StyleRevision = InStyleRevision;
	++StyleEpoch;
	Tiles.Reset();
	Pending.Reset();
	++Revision;
}

FVoxelMapTileKey FVoxelMapTileCache::ParentKey(const FVoxelMapTileKey& InKey, const int32 InParentStep)
{
	const int64 ParentCells = static_cast<int64>(TileSide) * InParentStep;
	return { FIntPoint(
		static_cast<int32>(FMath::FloorToInt64(static_cast<double>(InKey.Coordinate.X) * TileSide * InKey.Step / ParentCells)),
		static_cast<int32>(FMath::FloorToInt64(static_cast<double>(InKey.Coordinate.Y) * TileSide * InKey.Step / ParentCells))), InParentStep };
}

FBox2f FVoxelMapTileCache::ParentUV(const FVoxelMapTileKey& InKey, const FVoxelMapTileKey& InParent)
{
	const double Scale = static_cast<double>(InKey.Step) / InParent.Step;
	const double MinX = static_cast<double>(InKey.Coordinate.X) * Scale - InParent.Coordinate.X;
	const double MinY = static_cast<double>(InKey.Coordinate.Y) * Scale - InParent.Coordinate.Y;
	return FBox2f(FVector2f(MinX, 1.0 - MinY - Scale), FVector2f(MinX + Scale, 1.0 - MinY));
}

bool FVoxelMapTileCache::FindDrawBrush(const FVoxelMapTileKey& InKey, FSlateBrush& OutBrush) const
{
	if (const FSlateBrush* Brush = FindBrush(InKey))
	{
		OutBrush = *Brush;
		return true;
	}
	for (const int32 Step : { 8, 16, 32, 128, 512 })
	{
		if (Step <= InKey.Step)
		{
			continue;
		}
		const FVoxelMapTileKey Parent = ParentKey(InKey, Step);
		if (const FSlateBrush* Brush = FindBrush(Parent))
		{
			OutBrush = *Brush;
			OutBrush.SetUVRegion(ParentUV(InKey, Parent));
			return true;
		}
	}
	return false;
}

void FVoxelMapTileCache::ApplyTile(
	const FVoxelMapTileKey& InKey,
	FVoxelTaskResult&& InResult)
{
	check(IsInGameThread());
	Pending.Remove(InKey);
	if (!InResult.bSuccess || !InResult.CustomPayload)
	{
		if (!InResult.bCanceled && !InResult.Error.IsEmpty() && !InResult.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Voxel map tile %d,%d step %d: %s"),
				InKey.Coordinate.X, InKey.Coordinate.Y, InKey.Step, *InResult.Error);
		}
		return;
	}
	const FMapTilePixels* Pixels = static_cast<const FMapTilePixels*>(InResult.CustomPayload.Get());
	if (Pixels->Pixels.Num() != TileSide * TileSide)
	{
		return;
	}
	FTile Tile;
	Tile.Texture.Reset(UTexture2D::CreateTransient(TileSide, TileSide, PF_B8G8R8A8));
	if (!Tile.Texture)
	{
		return;
	}
	Tile.Texture->SRGB = true;
	Tile.Texture->Filter = TF_Nearest;
	Tile.Texture->NeverStream = true;
	void* Buffer = Tile.Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Buffer, Pixels->Pixels.GetData(), Pixels->Pixels.Num() * sizeof(FColor));
	Tile.Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Tile.Texture->UpdateResource();
	Tile.Brush.SetResourceObject(Tile.Texture.Get());
	Tile.Brush.ImageSize = FVector2D(TileSide, TileSide);
	Tile.LastUse = ++Sequence;
	Tiles.Add(InKey, MoveTemp(Tile));
	while (Tiles.Num() > 64)
	{
		EvictOldest();
	}
	++Revision;
}

void FVoxelMapTileCache::EvictOldest()
{
	const FVoxelMapTileKey* Oldest = nullptr;
	uint64 OldestUse = MAX_uint64;
	for (const TPair<FVoxelMapTileKey, FTile>& Pair : Tiles)
	{
		if (Pair.Value.LastUse < OldestUse)
		{
			Oldest = &Pair.Key;
			OldestUse = Pair.Value.LastUse;
		}
	}
	if (Oldest)
	{
		Tiles.Remove(*Oldest);
	}
}
