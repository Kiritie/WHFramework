#include "Ability/Inventory/InventoryClientReceiptLedger.h"

void FInventoryClientReceiptLedger::Record(const FInventoryClientRequestReceipt& InReceipt)
{
	check(IsInGameThread());
	if (InReceipt.ConnectionId.IsValid() && InReceipt.Result.RequestId)
		Receipts.FindOrAdd(InReceipt.ConnectionId).Add(InReceipt.Result.RequestId, InReceipt);
}

bool FInventoryClientReceiptLedger::Find(const FGuid& InConnectionId, uint64 InRequestId, FInventoryClientRequestReceipt& OutReceipt) const
{
	check(IsInGameThread());
	const auto* Connection = Receipts.Find(InConnectionId);
	const FInventoryClientRequestReceipt* Receipt = Connection ? Connection->Find(InRequestId) : nullptr;
	if (!Receipt) return false;
	OutReceipt = *Receipt;
	return true;
}

bool FInventoryClientReceiptLedger::Consume(const FGuid& InConnectionId, uint64 InRequestId, FInventoryClientRequestReceipt& OutReceipt)
{
	if (!Find(InConnectionId, InRequestId, OutReceipt)) return false;
	auto* Connection = Receipts.Find(InConnectionId);
	Connection->Remove(InRequestId);
	if (Connection->IsEmpty()) Receipts.Remove(InConnectionId);
	return true;
}

TArray<FInventoryClientRequestReceipt> FInventoryClientReceiptLedger::GetReceipts() const
{
	check(IsInGameThread());
	TArray<FInventoryClientRequestReceipt> Result;
	for (const auto& Connection : Receipts)
		for (const auto& Entry : Connection.Value) Result.Add(Entry.Value);
	return Result;
}
