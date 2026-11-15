#include "RetailPreview.hpp"
#include "Extractor_t8/RetailRuntime.hpp"

namespace RetailT8Preview {
namespace RT=RetailT8Runtime;
State Read(GameProcess& process,GameData& game,const GameInfo& info,uint8_t slot) {
    if(slot>1)throw std::out_of_range("Preview supports P1/P2 only");
    ImporterT8 importer(process,game,&info);
    State state;state.player=importer.GetCharacterAddress(slot);
    if(!state.player)throw std::runtime_error("Retail player is not loaded");
    state.offsets=importer.Offsets();
    state.root=RT::Read<uint64_t>(process,state.player+state.offsets.moveset);
    state.moves=RT::Read<uint64_t>(process,state.root+0x230);
    state.count=RT::Read<uint64_t>(process,state.root+0x238);
    if(!RT::Pointer(state.moves) || !state.count || state.count>100000)throw std::runtime_error("Invalid preview move table");
    state.character=RT::Read<uint32_t>(process,state.player+state.offsets.character);
    const auto current=RT::Read<uint64_t>(process,state.player+state.offsets.currentMove);
    if(current>=state.moves && current-state.moves<state.count*0x448 && (current-state.moves)%0x448==0)
        state.current=static_cast<uint32_t>((current-state.moves)/0x448);
    return state;
}
uint32_t Queue(GameProcess& process,GameData& game,const GameInfo& info,uint8_t slot,gameAddr expectedRoot,uint32_t move) {
    GameProcess::WriteBatch transaction(process);
    auto state=Read(process,game,info,slot);
    if(!expectedRoot || state.root!=expectedRoot)throw std::runtime_error("Player moveset changed; import this document again");
    const auto requested=move;
    if(move>=0x8000 && move<0x8000+60)move=RT::Read<uint16_t>(process,state.root+0xa8+2*(move-0x8000));
    if(move>=state.count)throw std::out_of_range("Preview move is outside the imported moveset");
    auto guard=[&](gameAddr address,const auto& value) {if(!process.ExpectBytes(address,&value,sizeof(value)))throw std::runtime_error("Cannot guard move preview");};
    guard(state.player+state.offsets.moveset,state.root);guard(state.root+0x230,state.moves);guard(state.root+0x238,state.count);guard(state.player+state.offsets.character,state.character);
    const auto registrySlot=process.mainModule.address+game.GetValue("registry_rva");
    const auto keySlot=process.mainModule.address+game.GetValue("round_key_rva");
    const auto registry=RT::Read<uint64_t>(process,registrySlot);const auto key=RT::Read<uint32_t>(process,keySlot);
    const auto map=RT::Read<uint64_t>(process,registry+0x10);
    const auto node=RT::Lookup(process,RT::Read<uint64_t>(process,map+0x100),RT::Read<uint64_t>(process,map+0x110),RT::Read<uint64_t>(process,map+0x128),key,false);
    if(!node)throw std::runtime_error("Preview round is no longer available");
    const auto round=RT::Read<uint64_t>(process,node+0x18);
    guard(registrySlot,registry);guard(keySlot,key);guard(registry+0x10,map);guard(node+0x18,round);
    const auto current=RT::Read<uint64_t>(process,state.player+state.offsets.currentMove);guard(state.player+state.offsets.currentMove,current);
    const auto length=RT::Read<uint32_t>(process,current+0x120);
    for(unsigned i=1;i<5;++i) {
        auto old=RT::Read<uint64_t>(process,state.player+state.offsets.moveset+8*i);guard(state.player+state.offsets.moveset+8*i,old);
        if(!process.writeUInt64(state.player+state.offsets.moveset+8*i,state.root))throw std::runtime_error("Cannot stage preview context");
    }
    if(!process.writeUInt32(state.player+state.offsets.nextMoveId,requested) || !process.writeUInt64(state.player+state.offsets.nextMove,state.moves+move*0x448) ||
        !process.writeUInt32(state.player+state.offsets.currentMoveTimer,length))throw std::runtime_error("Cannot stage preview move");
    GameProcess::PauseGuard pause(process);
    auto result=transaction.Commit();
    if(!result.ok)throw std::runtime_error(result.rollbackComplete ? "Preview state changed or write failed; no move queued" : "Preview rollback failed");
    return move;
}
}
