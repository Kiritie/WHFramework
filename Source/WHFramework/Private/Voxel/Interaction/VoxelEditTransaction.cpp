#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
bool FVoxelEditTransaction::Build(const FVoxelWorldRuntime&W,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&Shapes,
    const FVoxelTraceResult&H,EVoxelEditAction Action,uint16 PlaceType,const FVector&View,double Size,FVoxelInteractionPlan&O,FString&Error)
{
    if(!W.IsServer()||H.Status!=EVoxelTraceStatus::Hit||H.bStartedInside||!R.IsValid(H.State)||Size<=0)
    {Error=TEXT("Invalid interaction target");return false;}
    FVoxelInteractionPlan P;TMap<FIntVector,int32>Map;
    auto Read=[&](const FIntVector&Pos,FVoxelBlockState&State)->bool
    {if(const int32*I=Map.Find(Pos)){State=P.Cells[*I].Value;return true;}return W.TryGetBlock(Pos,State);};
    auto Write=[&](const FIntVector&Pos,FVoxelBlockState State)->bool
    {
        if(!R.IsValid(State))return false;
        if(int32*I=Map.Find(Pos)){P.Cells[*I].Value=State;return true;}
        FVoxelBlockState Old;if(!W.TryGetBlock(Pos,Old))return false;
        FVoxelCellEdit E;E.Position=Pos;E.Expected=Old;E.Value=State;Map.Add(Pos,P.Cells.Add(E));return true;
    };
    auto Supports=[&](const FIntVector&Pos,uint8 Face)->bool
    {FVoxelBlockState B;if(!Read(Pos,B)||B.IsAir())return false;const auto*D=R.Find(B.TypeId);
        return D&&D->bSolid&&(Shapes.Get(D->Shape,B.State).OcclusionMask&(1u<<Face));};
    auto Replaceable=[&](const FIntVector&Pos)->bool
    {FVoxelBlockState B;if(!Read(Pos,B))return false;const auto*D=R.Find(B.TypeId);return D&&(B.IsAir()||D->bReplaceable);};
    const auto*HitDef=R.Find(H.State.TypeId);FIntVector Target=H.Index;
    uint8 Yaw=uint8((FMath::RoundToInt(FMath::Atan2(View.Y,View.X)/(PI*.5))+4)&3);
    if(Action==EVoxelEditAction::BreakPulse)
    {
        if(!HitDef->bBreakable){Error=TEXT("Block is not breakable");return false;}
        if(HitDef->Shape==EVoxelShapeKind::Door)
        {
            FIntVector Lower=Target-FIntVector(0,0,(H.State.State&8)?1:0),Upper=Lower+FIntVector(0,0,1);
            FVoxelBlockState A,B;if(!Read(Lower,A)||!Read(Upper,B)||A.TypeId!=H.State.TypeId||B.TypeId!=A.TypeId||
                (A.State&8)||(B.State&8)==0||(A.State^B.State)!=8){Error=TEXT("Door pair is incomplete");return false;}
            if(!Write(Lower,{})||!Write(Upper,{}))return false;
        }
        else if(!Write(Target,{}))return false;
        P.DropID=HitDef->DropAssetID;P.DropCount=HitDef->DropCount;
        if(HitDef->Shape==EVoxelShapeKind::Slab&&(H.State.State&0x800))P.DropCount*=2;
        // 不丢弃有内容的容器：先取空，再破坏。没有隐式销毁 Inventory 的路径。
        if(HitDef->EntityKind==1)
        {
            const FVoxelSectionKey Key=VoxelCoord::Section(Target);const auto*S=W.FindSection(FIntVector(Key.X,Key.Y,Key.Z));const auto*B=S->Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(Target)));
            TArray<FVoxelItemStack>Slots;if(!B||!FVoxelBlockEntityCodec::DecodeContainer(*B,Slots))return false;
            for(const auto&I:Slots)if(I.Count>0){Error=TEXT("Empty the container before breaking it");return false;}
        }
    }
    else if(Action==EVoxelEditAction::Use)
    {
        if(HitDef->Shape==EVoxelShapeKind::Door)
        {
            FIntVector Lower=Target-FIntVector(0,0,(H.State.State&8)?1:0),Upper=Lower+FIntVector(0,0,1);FVoxelBlockState A,B;
            if(!Read(Lower,A)||!Read(Upper,B)||A.TypeId!=B.TypeId||A.TypeId!=H.State.TypeId||(A.State^B.State)!=8||(A.State&8))
            {Error=TEXT("Door pair is incomplete");return false;}
            A.State^=16;B.State^=16;if(!Write(Lower,A)||!Write(Upper,B))return false;
        }
        else if(HitDef->Shape==EVoxelShapeKind::Trapdoor)
        {auto S=H.State;S.State^=16;if(!Write(Target,S))return false;}
        else if(HitDef->EntityKind==2)
        {
            const FVoxelSectionKey Key=VoxelCoord::Section(Target);const auto*S=W.FindSection(FIntVector(Key.X,Key.Y,Key.Z));const auto*B=S->Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(Target)));
            FVoxelEntityEdit E;E.Position=Target;if(!B||!FVoxelBlockEntityCodec::IncrementCounter(*B,E.Value))return false;P.Entities.Add(E);
        }
        else{Error=TEXT("This block has no Use action; container operations use explicit slot intents");return false;}
    }
    else if(Action==EVoxelEditAction::Place)
    {
        const auto*D=R.Find(PlaceType);if(!D||PlaceType==0){Error=TEXT("Unknown placeable block");return false;}
        Target=HitDef->bReplaceable?H.Index:H.PlacementIndex;FVoxelBlockState State{PlaceType,0};
        bool MergeSlab=HitDef->Shape==EVoxelShapeKind::Slab&&D->Shape==EVoxelShapeKind::Slab&&PlaceType==H.State.TypeId&&
            !(H.State.State&0x800)&&((!(H.State.State&8)&&H.Normal.Z>.5)||((H.State.State&8)&&H.Normal.Z<-.5));
        if(MergeSlab){Target=H.Index;State.State=0x800;}
        else
        {
            if(!Replaceable(Target)){Error=TEXT("Placement cell is occupied or unavailable");return false;}
            double Fraction=H.Point.Z/Size-FMath::FloorToDouble(H.Point.Z/Size);
            switch(D->Shape)
            {
                case EVoxelShapeKind::FullCube:case EVoxelShapeKind::Stair:case EVoxelShapeKind::Door:
                case EVoxelShapeKind::Trapdoor:State.State=Yaw;break;
                default:break;
            }
            if(D->Shape==EVoxelShapeKind::Slab||D->Shape==EVoxelShapeKind::Stair||D->Shape==EVoxelShapeKind::Trapdoor)
                State.State|=(H.Normal.Z<-.5||(FMath::Abs(H.Normal.Z)<.5&&Fraction>.5))?8:0;
            if(D->Shape==EVoxelShapeKind::Door)
            {
                if(!Supports(Target-FIntVector(0,0,1),4)||!Replaceable(Target+FIntVector(0,0,1)))
                {Error=TEXT("Door needs a full support and two free cells");return false;}
                State.State&=~8;FVoxelBlockState Upper=State;Upper.State|=8;
                if(!Write(Target+FIntVector(0,0,1),Upper))return false;
            }
            if(D->Shape==EVoxelShapeKind::Ladder||D->Shape==EVoxelShapeKind::Torch)
            {
                if(H.Normal.Z>.5&&D->Shape==EVoxelShapeKind::Torch)
                {State.State=4;if(!Supports(Target-FIntVector(0,0,1),4))return false;}
                else
                {
                    uint8 Face=H.Normal.X>.5?0:H.Normal.X<-.5?1:H.Normal.Y>.5?2:H.Normal.Y<-.5?3:255;
                    if(Face==255||!Supports(Target-VoxelCoord::Direction(Face),Face))
                    {Error=TEXT("Wall attachment needs a full supporting face");return false;}
                    State.State=Face==0?0:Face==2?1:Face==1?2:3;
                }
            }
            if(D->Shape==EVoxelShapeKind::CrossPlant&&!Supports(Target-FIntVector(0,0,1),4))
            {Error=TEXT("Plant needs a solid supporting block");return false;}
        }
        if(!Write(Target,State))return false;P.Cost=1;
    }
    else{Error=TEXT("Action is not a block mutation");return false;}
    // 有支撑依赖的方块不自动吞掉掉落：移除其支撑时，要求玩家先拆除依附块。
    TArray<FIntVector>Changed;Map.GetKeys(Changed);
    for(const FIntVector&Center:Changed)
    {
        const FIntVector Above=Center+FIntVector(0,0,1);FVoxelBlockState Plant;
        if(!Read(Above,Plant)){Error=TEXT("Load the adjacent section before removing its plant support");return false;}
        const auto*Definition=R.Find(Plant.TypeId);
        if(!Definition||Plant.IsAir()||Definition->Shape!=EVoxelShapeKind::CrossPlant||Definition->DropCount!=0||Definition->EntityKind!=0||Supports(Center,4))continue;
        if(P.Cells.Num()>=8192||!Write(Above,{})){Error=TEXT("Plant support cleanup exceeds the bounded edit plan");return false;}
    }
    Map.GetKeys(Changed);
    for(const FIntVector&Center:Changed)for(uint8 F=0;F<6;++F)
    {
        FIntVector Q=Center+VoxelCoord::Direction(F);FVoxelBlockState B;
        if(!Read(Q,B)){Error=TEXT("Neighbor section is not ready");return false;}
        if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);FIntVector Support;uint8 SupportFace=4;bool Check=false;
        if(D->Shape==EVoxelShapeKind::CrossPlant||(D->Shape==EVoxelShapeKind::Door&&!(B.State&8))||
            (D->Shape==EVoxelShapeKind::Torch&&(B.State&7)==4)){Support=Q-FIntVector(0,0,1);Check=true;}
        else if(D->Shape==EVoxelShapeKind::Ladder||D->Shape==EVoxelShapeKind::Torch)
        {SupportFace=FVoxelShapeRegistry::RotateFace(0,uint8(B.State&7));Support=Q-VoxelCoord::Direction(SupportFace);Check=true;}
        if(Check&&!Supports(Support,SupportFace)){Error=TEXT("Remove attached block before its support");return false;}
    }
    // Pane 派生连接只由服务器根据最终候选世界计算。相邻 Pane 的连接也包含在同一事务。
    TSet<FIntVector>Panes;for(const auto&C:Changed){Panes.Add(C);for(uint8 F=0;F<4;++F)Panes.Add(C+VoxelCoord::Direction(F));}
    for(const auto&Q:Panes)
    {
        FVoxelBlockState B;if(!Read(Q,B)){Error=TEXT("Pane neighbor is unavailable");return false;}
        if(B.IsAir()||R.Find(B.TypeId)->Shape!=EVoxelShapeKind::Pane)continue;uint8 Mask=0;
        for(uint8 F=0;F<4;++F){FVoxelBlockState N;if(!Read(Q+VoxelCoord::Direction(F),N))return false;
            const auto*D=R.Find(N.TypeId);if(D&&!N.IsAir()&&(D->Shape==EVoxelShapeKind::Pane||
                (D->bSolid&&(Shapes.Get(D->Shape,N.State).OcclusionMask&(1u<<(F^1))))))Mask|=uint8(1u<<F);}
        B.State=VoxelState::WithConnections(0,Mask);if(!Write(Q,B))return false;
    }
    P.Cells.RemoveAll([](const FVoxelCellEdit&E){return E.Expected==E.Value;});
    if(P.Cells.IsEmpty()&&P.Entities.IsEmpty()){Error=TEXT("No state change");return false;}
    O=MoveTemp(P);Error.Reset();return true;
}

bool FVoxelEditTransaction::ValidateBatch(const FVoxelWorldRuntime&W,const FVoxelRegistrySnapshot&R,
    const FVoxelShapeRegistry&Shapes,TArray<FVoxelCellEdit>&Cells,FString&Error)
{
    TArray<FVoxelCellEdit>Candidate=Cells;TMap<FIntVector,int32>Map;
    for(int32 I=0;I<Candidate.Num();++I)
    {auto&E=Candidate[I];FVoxelBlockState Actual;if(Map.Contains(E.Position)||!R.IsValid(E.Value)||!W.TryGetBlock(E.Position,Actual)||Actual!=E.Expected)
        {Error=TEXT("Duplicate, stale or invalid batch cell");return false;}Map.Add(E.Position,I);
        if(Actual!=E.Value)
        {const FVoxelSectionKey Key=VoxelCoord::Section(E.Position);const auto*S=W.FindSection(FIntVector(Key.X,Key.Y,Key.Z));const auto*BE=S->Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(E.Position)));
            if(BE&&BE->Kind==1){TArray<FVoxelItemStack>Items;if(!FVoxelBlockEntityCodec::DecodeContainer(*BE,Items))return false;
                for(const auto&Item:Items)if(Item.Count>0){Error=TEXT("Batch would destroy nonempty container");return false;}}}}
    auto Read=[&](const FIntVector&P,FVoxelBlockState&B){if(const int32*I=Map.Find(P)){B=Candidate[*I].Value;return true;}return W.TryGetBlock(P,B);};
    auto Supports=[&](const FIntVector&P,uint8 F){FVoxelBlockState B;if(!Read(P,B)||B.IsAir())return false;const auto*D=R.Find(B.TypeId);
        return D&&D->bSolid&&(Shapes.Get(D->Shape,B.State).OcclusionMask&(1u<<F));};
    TSet<FIntVector>Inspect;for(const auto&E:Candidate){Inspect.Add(E.Position);for(uint8 F=0;F<6;++F)Inspect.Add(E.Position+VoxelCoord::Direction(F));}
    for(const auto&P:Inspect)
    {
        FVoxelBlockState B;if(!Read(P,B)){Error=TEXT("Batch dependency section is unavailable");return false;}if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);
        if(D->Shape==EVoxelShapeKind::Door)
        {FIntVector Q=P+FIntVector(0,0,(B.State&8)?-1:1);FVoxelBlockState Other;
            if(!Read(Q,Other)||Other.TypeId!=B.TypeId||(Other.State^B.State)!=8){Error=TEXT("Batch has an incomplete door");return false;}}
        if(D->Shape==EVoxelShapeKind::CrossPlant||(D->Shape==EVoxelShapeKind::Door&&!(B.State&8))||(D->Shape==EVoxelShapeKind::Torch&&(B.State&7)==4))
        {if(!Supports(P-FIntVector(0,0,1),4)){Error=TEXT("Batch removes required floor support");return false;}}
        else if(D->Shape==EVoxelShapeKind::Ladder||D->Shape==EVoxelShapeKind::Torch)
        {uint8 F=FVoxelShapeRegistry::RotateFace(0,uint8(B.State&7));if(!Supports(P-VoxelCoord::Direction(F),F)){Error=TEXT("Batch removes required wall support");return false;}}
        if(D->Shape==EVoxelShapeKind::Pane)
        {
            uint8 Mask=0;for(uint8 F=0;F<4;++F){FVoxelBlockState N;if(!Read(P+VoxelCoord::Direction(F),N))return false;const auto*ND=R.Find(N.TypeId);
                if(ND&&!N.IsAir()&&(ND->Shape==EVoxelShapeKind::Pane||(ND->bSolid&&(Shapes.Get(ND->Shape,N.State).OcclusionMask&(1u<<(F^1))))))Mask|=uint8(1u<<F);}
            B.State=VoxelState::WithConnections(0,Mask);
            if(int32*I=Map.Find(P))Candidate[*I].Value=B;
            else{FVoxelBlockState Old;if(!W.TryGetBlock(P,Old))return false;if(Old!=B){FVoxelCellEdit E;E.Position=P;E.Expected=Old;E.Value=B;Map.Add(P,Candidate.Add(E));}}
        }
    }
    Candidate.RemoveAll([](const FVoxelCellEdit&E){return E.Expected==E.Value;});Cells=MoveTemp(Candidate);Error.Reset();return true;
}
