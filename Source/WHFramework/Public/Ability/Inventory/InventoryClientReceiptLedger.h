#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryCommandTypes.h"

/** 原连接上的已退役请求；ConfirmationPending 表示结果仍需核对。 */
struct WHFRAMEWORK_API FInventoryClientRequestReceipt
{
	FGuid ConnectionId;
	FGuid StoreId;
	FGuid WorldEpoch;
	FGuid SourceInventoryId;
	FName DomainAction;
	FInventoryCommandResult Result;
};

/** 本地玩家会话持有的非权威回执，不拥有世界或库存状态。 */
class WHFRAMEWORK_API FInventoryClientReceiptLedger
{
public:
	void Record(const FInventoryClientRequestReceipt& InReceipt);
	bool Find(const FGuid& InConnectionId, uint64 InRequestId, FInventoryClientRequestReceipt& OutReceipt) const;
	bool Consume(const FGuid& InConnectionId, uint64 InRequestId, FInventoryClientRequestReceipt& OutReceipt);
	TArray<FInventoryClientRequestReceipt> GetReceipts() const;

private:
	TMap<FGuid, TMap<uint64, FInventoryClientRequestReceipt>> Receipts;
};
