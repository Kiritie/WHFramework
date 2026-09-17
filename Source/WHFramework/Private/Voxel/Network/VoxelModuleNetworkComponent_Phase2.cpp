#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Rendering/VoxelWorldView.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
namespace
{
bool NearObserver(const AActor* A,double CellCm,const VoxelGen::Box& B,double XY,double Z)
{
    if(!A||!FMath::IsFinite(CellCm)||CellCm<=0)return false;
    const FVector P=A->GetActorLocation();
    const double X0=double(B.min.x)*CellCm,X1=double(B.max.x)*CellCm;
    const double Y0=double(B.min.y)*CellCm,Y1=double(B.max.y)*CellCm;
    const double Z0=double(B.min.z)*CellCm,Z1=double(B.max.z)*CellCm;
    const double DX=FMath::Max(FMath::Max(X0-P.X,P.X-X1),0.0);
    const double DY=FMath::Max(FMath::Max(Y0-P.Y,P.Y-Y1),0.0);
    const double DZ=FMath::Max(FMath::Max(Z0-P.Z,P.Z-Z1),0.0);
    return DX*DX+DY*DY<=XY*XY&&DZ<=Z;
}
VoxelGen::Box FineBounds(const FVoxelSectionKey& K)
{
    return {{K.X*16,K.Y*16,K.Z*16},{K.X*16+16,K.Y*16+16,K.Z*16+16}};
}
void WriteProxyKey(FVoxelByteWriter& W,VoxelView::Key K)
{
    W.I32(K.coordinate.x);W.I32(K.coordinate.y);W.I32(K.coordinate.z);W.U8(K.level);
}
VoxelView::Key ReadProxyKey(FVoxelByteReader& R)
{
    VoxelView::Key K;K.coordinate.x=R.I32();K.coordinate.y=R.I32();K.coordinate.z=R.I32();K.level=R.U8();return K;
}
}
bool UVoxelModuleNetworkComponent::RequestProxy(const FVoxelProxyRequest& Q)
{
    auto* PC=Controller();
    if(!PC||PC->HasAuthority()||!PC->IsLocalController()||!IsSessionReady()||
       (Phase2.ClientPending.size()>=64&&!Phase2.ClientPending.count(Q.Key))||Transfer.QueuedBytes()>256*1024)return false;
    TArray<uint8> B;if(!FVoxelProxyData::EncodeRequest(Q,B)||!Send(EVoxelMessage::ProxyRequest,B))return false;
    Phase2.ClientPending[Q.Key]=Q.Serial;return true;
}
bool UVoxelModuleNetworkComponent::DeliverProxy(const FVoxelProxyReply& Reply)
{
    auto* PC=Controller();
    if(!PC||!PC->HasAuthority()||!IsSessionReady())return false;
    auto It=Phase2.Outstanding.find(Reply.Key);
    if(It==Phase2.Outstanding.end()||It->second!=Reply.Serial)return false;
    Phase2.Outstanding.erase(It);
    if(Phase2.Deferred.Num()>=16)return false; // Client retries; no unbounded response queue.
    Phase2.Deferred.Add(Reply);return true;
}
void UVoxelModuleNetworkComponent::PumpPhase2()
{
    auto* PC=Controller();if(!PC||!IsSessionReady())return;
    if(!PC->HasAuthority())return;
    // Subscriptions stay valid for the session. Capacity eviction sends an explicit invalidation first.
    // Existing fine/atomic transfer remains ahead of coarse presentation replies.
    if(Phase2.Deferred.IsEmpty()||!DirtyTransactions.IsEmpty()||PendingBatches.Num()>=4||Transfer.QueuedBytes()>256*1024)return;
    TArray<uint8> B;
    if(!FVoxelProxyData::EncodeReply(Phase2.Deferred[0],B)){Fail(TEXT("Proxy reply encode failed"));return;}
    if(Send(EVoxelMessage::ProxyReply,B))Phase2.Deferred.RemoveAt(0);
}
void UVoxelModuleNetworkComponent::SendFineInterest()
{
    auto* M=Module.Get();auto* PC=Controller();
    if(!M||!PC||PC->HasAuthority()||!PC->IsLocalController()||!IsSessionReady())return;
    AActor* Observer=GetAuthorizedObserver();if(!Observer)return;
    TArray<FVoxelSectionKey> All,Keys;M->GetFineViewKeys(All);
    for(const auto& K:All)
        if(VoxelCoord::IsValidSection(K,M->GetManifest().Settings.MinZ,M->GetManifest().Settings.MaxZ)&&
           NearObserver(Observer,M->BlockSize(),FineBounds(K),4800,4800))Keys.Add(K);
    if(Keys.Num()>8192){Fail(TEXT("Fine interest exceeds phase2 capacity"));return;}
    FVoxelByteWriter W(100000);W.U32(3);W.U16(uint16(Keys.Num()));
    for(const auto& K:Keys)FVoxelNetworkCodec::WriteKey(W,K);
    TArray<uint8> B;if(!W.Finish(B)){Fail(TEXT("Fine interest encode failed"));return;}Send(EVoxelMessage::Interest,B);
}
void UVoxelModuleNetworkComponent::ResetPhase2()
{
    if(auto* M=Module.Get())M->ClearRemoteFineDemand(this);
    Phase2.Reset();
}
bool UVoxelModuleNetworkComponent::HandlePhase2(const FVoxelWireMessage& W,bool Server)
{
    auto* M=Module.Get();auto* View=M?M->GetWorldView():nullptr;
    if(Server&&W.Kind==EVoxelMessage::Interest)
    {
        FVoxelByteReader R(W.Payload);const uint32 Version=R.U32();const uint16 N=R.U16();
        if(!M||Version!=3||N>8192){Fail(TEXT("Invalid phase2 fine interest"));return true;}
        TSet<FVoxelSectionKey> Next;AActor* Observer=GetAuthorizedObserver();
        for(uint16 I=0;I<N;++I)
        {
            const auto K=FVoxelNetworkCodec::ReadKey(R);
            if(!VoxelCoord::IsValidSection(K,M->GetManifest().Settings.MinZ,M->GetManifest().Settings.MaxZ)||Next.Contains(K))
            {Fail(TEXT("Invalid or duplicate fine section"));return true;}
            // Movement can make a previous legitimate request stale. Ignore its remote tail, do not allocate it.
            if(NearObserver(Observer,M->BlockSize(),FineBounds(K),4800,4800))Next.Add(K);
        }
        if(!R.End()){Fail(TEXT("Malformed fine interest"));return true;}
        Phase2.Fine=MoveTemp(Next);RefreshInterest();return true;
    }
    if(Server&&W.Kind==EVoxelMessage::ProxyRequest)
    {
        FVoxelProxyRequest Q;
        if(!M||!View||!FVoxelProxyData::DecodeRequest(W.Payload,Q))
        {Fail(TEXT("Invalid proxy request"));return true;}
        const double Now=FPlatformTime::Seconds();
        Phase2.Tokens=FMath::Min(64.0,Phase2.Tokens+FMath::Clamp(Now-Phase2.TokenTime,0.0,1.0)*32.0);Phase2.TokenTime=Now;
        const bool Authorized=NearObserver(GetAuthorizedObserver(),M->BlockSize(),Q.Key.Bounds(),32000,19200);
        if(!Authorized||Phase2.Tokens<1||Phase2.Outstanding.size()>=4||Phase2.Deferred.Num()>=12)
        {
            // A bounded Retry is not a grant to load the requested area.
            if(Phase2.Deferred.Num()<16){FVoxelProxyReply X;X.Key=Q.Key;X.Serial=Q.Serial;X.Mode=EVoxelProxyMode::Retry;Phase2.Deferred.Add(X);}
            return true;
        }
        if(Phase2.Subscribed.size()>=16384&&!Phase2.Subscribed.count(Q.Key))
        {
            auto Old=Phase2.Subscribed.end();
            for(auto It=Phase2.Subscribed.begin();It!=Phase2.Subscribed.end();++It)
                if(!Phase2.Outstanding.count(It->first)&&(Old==Phase2.Subscribed.end()||It->second<Old->second))Old=It;
            if(Old==Phase2.Subscribed.end())return true;
            FVoxelByteWriter Notice(32);Notice.U16(1);WriteProxyKey(Notice,Old->first);TArray<uint8> Payload;
            if(!Notice.Finish(Payload)||!Send(EVoxelMessage::ProxyInvalidate,Payload))return true;
            Phase2.Subscribed.erase(Old);
        }
        --Phase2.Tokens;Phase2.Subscribed[Q.Key]=Now;Phase2.Outstanding[Q.Key]=Q.Serial;
        if(!View->ServeProxy(Q,this))
        {FVoxelProxyReply X;X.Key=Q.Key;X.Serial=Q.Serial;X.Mode=EVoxelProxyMode::Retry;DeliverProxy(X);}
        return true;
    }
    if(!Server&&W.Kind==EVoxelMessage::ProxyReply)
    {
        FVoxelProxyReply R;
        if(!M||!View||!FVoxelProxyData::DecodeReply(W.Payload,uint32(M->GetGenerator()->GetConfig().Catalog->blocks.size()),R))
        {Fail(TEXT("Invalid proxy response"));return true;}
        auto It=Phase2.ClientPending.find(R.Key);
        if(It==Phase2.ClientPending.end()||It->second!=R.Serial)return true;
        Phase2.ClientPending.erase(It);View->ReceiveProxy(R);return true;
    }
    if(!Server&&W.Kind==EVoxelMessage::ProxyInvalidate)
    {
        FVoxelByteReader R(W.Payload);const uint16 N=R.U16();
        if(!View||N>128){Fail(TEXT("Invalid proxy invalidation count"));return true;}
        std::vector<VoxelView::Key> Keys;Keys.reserve(N);
        for(uint16 I=0;I<N;++I){auto K=ReadProxyKey(R);if(!FVoxelProxyData::ValidKey(K)){Fail(TEXT("Invalid proxy invalidation key"));return true;}Keys.push_back(K);}
        if(!R.End()){Fail(TEXT("Malformed proxy invalidation"));return true;}
        for(const auto& K:Keys){Phase2.ClientPending.erase(K);View->InvalidateProxy(K);}return true;
    }
    if(W.Kind==EVoxelMessage::ProxyRequest||W.Kind==EVoxelMessage::ProxyReply||W.Kind==EVoxelMessage::ProxyInvalidate)
    {Fail(TEXT("Proxy message direction is not permitted"));return true;}
    return false;
}
void UVoxelModuleNetworkComponent::NotifyProxyEdit(const FVoxelEditBatch& Batch)
{
    if(!Controller()||!Controller()->HasAuthority()||!IsSessionReady())return;
    std::set<VoxelView::Key> Keys;
    for(const auto& S:Batch.Sections)for(uint8 L=1;L<=4;++L)
    {
        const int D=1<<L;VoxelView::Key K{{VoxelGen::FloorDiv(S.Key.X,D),VoxelGen::FloorDiv(S.Key.Y,D),VoxelGen::FloorDiv(S.Key.Z,D)},L};
        if(Phase2.Subscribed.count(K))Keys.insert(K);
    }
    if(Keys.empty())return;
    FVoxelByteWriter W(2048);W.U16(uint16(Keys.size()));for(auto K:Keys)WriteProxyKey(W,K);
    TArray<uint8> B;if(W.Finish(B))Send(EVoxelMessage::ProxyInvalidate,B);
}
