#include "Voxel/Interaction/VoxelInventoryTransaction.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
bool FVoxelInventoryTransaction::SetSilent(UAbilityInventorySlotBase&S,const FAbilityItem&Expected,const FAbilityItem&Next)
{
    check(IsInGameThread());if(!S.GetInventory()||!S.IsEnabled()||!S.GetItem().Equal(Expected)||Next.Count<0)return false;
    FAbilityItem N=Next.Count==0?FAbilityItem::Empty:Next;
    if(N.Count>0&&(!N.ID.IsValid()||!S.MatchItemLimit(N,true)||!S.MatchItemSplit(N,false)||N.Count>S.GetMaxVolume(N)||N.Level<0||N.Level>S.GetMaxLevel(N)))return false;
    N.Payload=&S;N.Handle=N.Match(Expected)?Expected.Handle:FGameplayAbilitySpecHandle();S.GetItem()=N;return true;
}
void FVoxelInventoryTransaction::RestoreSilent(UAbilityInventorySlotBase&S,const FAbilityItem&P)
{check(IsInGameThread());S.GetItem()=P;S.GetItem().Payload=&S;}
void FVoxelInventoryTransaction::Notify(UAbilityInventorySlotBase&S,const FAbilityItem&Previous)
{
    check(IsInGameThread());if(!S.GetInventory())return;FAbilityItem Old=Previous;
    if(S.GetItem().ID!=Old.ID||S.GetItem().Level!=Old.Level)
    {
        // Existing framework callback takes the old value. Delivery is deferred until both world and slot are committed.
        if(auto Agent=S.GetInventory()->GetOwnerAgent())Agent->OnPreChangeItem(Old);
        S.OnItemChanged(Old,true);
    }
    S.Refresh();
}
