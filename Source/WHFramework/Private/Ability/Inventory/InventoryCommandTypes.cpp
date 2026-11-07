#include "Ability/Inventory/InventoryCommandTypes.h"

bool FInventoryCommandResult::IsSuccess() const
{
    return Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial
        || Code == EInventoryResultCode::AlreadyCommitted;
}
