#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

// GT 独占的固定 33 层空间索引；记录顺序使用侵入式链，不增长全域 Map/稀疏数组。
// RecordType 提供 Section、Previous、Next；所有节点和记录均在授予后分配，最后引用交 Worker。
template<typename RecordType>
class TVoxelCollisionBodyIndex
{
private:
	struct FNode
	{
		TSharedPtr<FNode, ESPMode::ThreadSafe> Children[8];
		TSharedPtr<RecordType, ESPMode::ThreadSafe> Record;
	};

	static uint8 Branch(const FIntVector& InKey, const int32 InDepth)
	{
		const int32 Bit = 31 - InDepth;
		return (((uint32(InKey.X) ^ 0x80000000u) >> Bit) & 1u) |
			((((uint32(InKey.Y) ^ 0x80000000u) >> Bit) & 1u) << 1) |
			((((uint32(InKey.Z) ^ 0x80000000u) >> Bit) & 1u) << 2);
	}

public:
	TVoxelCollisionBodyIndex()
	{
	}

	TVoxelCollisionBodyIndex(const TVoxelCollisionBodyIndex&) = delete;
	TVoxelCollisionBodyIndex& operator=(const TVoxelCollisionBodyIndex&) = delete;

	RecordType* Find(const FIntVector& InKey) const
	{
		check(IsInGameThread());
		const FNode* Node = Root.Get();
		for (int32 Depth = 0; Depth < 32 && Node; ++Depth)
		{
			Node = Node->Children[Branch(InKey, Depth)].Get();
		}
		return Node ? Node->Record.Get() : nullptr;
	}

	RecordType* FindOrAdd(const FIntVector& InKey,
		FVoxelTerrainResources& InResources, const FVoxelGenerationConsumerKey& InConsumer,
		bool& bOutAdded)
	{
		check(IsInGameThread());
		bOutAdded = false;
		if (auto* Record = Find(InKey)) return Record;
		check(Count < MAX_int32);

		int32 MissingNodes = 0;
		const FNode* Node = Root.Get();
		for (int32 Depth = 0; Depth <= 32; ++Depth)
		{
			if (!Node) { MissingNodes = 33 - Depth; break; }
			if (Depth < 32) Node = Node->Children[Branch(InKey, Depth)].Get();
		}
		const uint64 NodeBytes = VoxelBoundedAllocation::WorkerSharedBytes<FNode>();
		const uint64 RecordBytes = VoxelBoundedAllocation::WorkerSharedBytes<RecordType>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!InResources.Acquire(InConsumer, EVoxelTerrainResourcePool::Resident,
			uint64(MissingNodes) * NodeBytes + RecordBytes, Lease, MissingNodes + 1)) return nullptr;

		// 全部新 backing 先准备完；容量不足不会改变现有索引、游标或记录。
		auto RecordLease = Lease->Split(RecordBytes);
		check(RecordLease);
		auto NewRecord = VoxelBoundedAllocation::MakeWorkerShared<RecordType>(MoveTemp(RecordLease));
		NewRecord->Section = InKey;
		TSharedPtr<FNode, ESPMode::ThreadSafe> Prepared[33];
		for (int32 Index = 0; Index < MissingNodes; ++Index)
		{
			auto NodeLease = Lease->Split(NodeBytes);
			check(NodeLease);
			Prepared[Index] = VoxelBoundedAllocation::MakeWorkerShared<FNode>(MoveTemp(NodeLease));
		}
		int32 PreparedCursor = 0;
		auto* Link = &Root;
		for (int32 Depth = 0; Depth <= 32; ++Depth)
		{
			if (!*Link) *Link = MoveTemp(Prepared[PreparedCursor++]);
			if (Depth < 32) Link = &(*Link)->Children[Branch(InKey, Depth)];
		}
		check(PreparedCursor == MissingNodes && !(*Link)->Record);
		(*Link)->Record = MoveTemp(NewRecord);
		auto* Record = (*Link)->Record.Get();
		Record->Next = Head;
		if (Head) Head->Previous = Record;
		Head = Record;
		if (!Cursor) Cursor = Record;
		++Count;
		bOutAdded = true;
		return Record;
	}

	RecordType* NextForScan()
	{
		check(IsInGameThread());
		auto* Record = Cursor;
		if (Cursor) Cursor = Cursor->Next ? Cursor->Next : Head;
		return Record;
	}

	void Remove(RecordType& InRecord)
	{
		check(IsInGameThread());
		TSharedPtr<FNode, ESPMode::ThreadSafe>* Links[33];
		Links[0] = &Root;
		for (int32 Depth = 0; Depth < 32; ++Depth)
		{
			check(*Links[Depth]);
			Links[Depth + 1] = &(*Links[Depth])->Children[Branch(InRecord.Section, Depth)];
		}
		check(*Links[32] && (*Links[32])->Record.Get() == &InRecord);
		if (InRecord.Previous) InRecord.Previous->Next = InRecord.Next;
		else Head = InRecord.Next;
		if (InRecord.Next) InRecord.Next->Previous = InRecord.Previous;
		if (Cursor == &InRecord) Cursor = InRecord.Next ? InRecord.Next : Head;
		InRecord.Previous = InRecord.Next = nullptr;
		--Count;
		auto RetiredRecord = MoveTemp((*Links[32])->Record);
		int32 PruneDepth = 33;
		for (int32 Depth = 32; Depth >= 0; --Depth)
		{
			bool bEmpty = !(*Links[Depth])->Record;
			for (int32 Child = 0; Child < 8; ++Child)
			{
				// 已确认下一层整条支路为空，保留其内部指针交一个 Worker 深析构。
				if (Depth < 32 && Child == Branch(InRecord.Section, Depth)) continue;
				bEmpty &= !(*Links[Depth])->Children[Child];
			}
			if (!bEmpty) break;
			PruneDepth = Depth;
		}
		TSharedPtr<FNode, ESPMode::ThreadSafe> RetiredPath;
		if (PruneDepth < 33) RetiredPath = MoveTemp(*Links[PruneDepth]);
	}

	RecordType* GetHead() const { return Head; }
	int32 Num() const { return Count; }

	void Reset()
	{
		check(IsInGameThread());
		Head = Cursor = nullptr;
		Count = 0;
		// 无容器遍历、无新增任务授予；根的 Worker deleter 在创建时已计费。
		Root.Reset();
	}

private:
	TSharedPtr<FNode, ESPMode::ThreadSafe> Root;
	RecordType* Head = nullptr;
	RecordType* Cursor = nullptr;
	int32 Count = 0;
};
