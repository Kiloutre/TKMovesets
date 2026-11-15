#include "AppTheme.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "EditorVisuals_t8Retail/EditorVisuals_t8Retail.hpp"
#include "Games.hpp"
#include "MovesetFile.hpp"
#include "FileJob.hpp"
#include "FileIO.hpp"
#include "MovesetFolders.hpp"
#include "LocalStorage.hpp"
#include "Extractor_t8/Extractor_t8.hpp"
#include "Settings.hpp"
#include "Localization.hpp"
#include <imgui_internal.h>
#include <fstream>
#include "constants.h"
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include "RetailPanm.hpp"
#include "T7AnimToRetailT8.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "Importer_t8/RetailLiveEdit.hpp"
#include <algorithm>
#include <functional>
#include <thread>
#include "RetailCommands.hpp"
#include "RetailNames.hpp"
#include "EditorVisuals_t8Retail/RetailFieldCodecs.hpp"

namespace {
namespace R=RetailT8Snapshot;
void Check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F action) { bool failed=false; try { action(); } catch(const std::exception&) {failed=true;} Check(failed,"Unsafe retail edit accepted"); }
template<class T> void Put(std::vector<Byte>& bytes,size_t offset,T value) { memcpy(bytes.data()+offset,&value,sizeof(value)); }
using FixturePatch=std::function<void(std::vector<Byte>&,size_t,const R::Entry&)>;
std::vector<Byte> Fixture(bool assets=true,const FixturePatch& patch={}) {
    auto align=[](size_t n) {return (n+7)&~size_t(7);};
    R::Header header{{'T','8','R','S'},assets?3U:1U,0x10000,sizeof(R::Header),align(sizeof(R::Header)+R::SourceHeaderSize),0};
    std::vector<Byte> bytes(header.directoryOffset+20*sizeof(R::Entry));
    Put<uint32_t>(bytes,header.sourceHeaderOffset+4,20261001);
    for(size_t i=0;i<20;++i) {
        auto spec=R::Tables[i]; uint64_t count=i==5?65537:2;
        R::Entry entry{0x100000+i*0x100000,count,align(bytes.size()),spec.stride,0};
        bytes.resize(entry.offset+entry.count*entry.stride);
        Put(bytes,header.sourceHeaderOffset+spec.pointerOffset,entry.address);
        Put(bytes,header.sourceHeaderOffset+spec.countOffset,count);
        Put(bytes,header.directoryOffset+i*sizeof(entry),entry);
        if(i==1) Put<uint32_t>(bytes,entry.offset+spec.stride,1100);
        if(i==6 || i==7) Put<uint64_t>(bytes,entry.offset+spec.stride,0x8000);
        if(i==9){Put<uint32_t>(bytes,entry.offset,32769);Put<uint32_t>(bytes,entry.offset+16,32769);}
        if(i==10 || i==11) Put<uint32_t>(bytes,entry.offset+spec.stride+8,1100);
        if(i==12) for(size_t row=0;row<count;++row) for(size_t offset:{0U,0x20U,0x58U,0x78U}) {
            const uint64_t key=0x123456789abcdef0ULL+offset;
            Put(bytes,entry.offset+row*spec.stride+offset,EditorT8Retail::Encode(uint32_t(row+offset),key));
            Put(bytes,entry.offset+row*spec.stride+offset+8,key);
        }
        if(patch) patch(bytes,i,entry);
    }
    if(assets) {
        bytes.resize(align(bytes.size())); const auto at=bytes.size();
        R::AssetDirectory directory{{'T','8','A','N'},1,at+sizeof(R::AssetDirectory),align(at+sizeof(R::AssetDirectory)+sizeof(R::AssetEntry)),0};
        bytes.resize(directory.dataOffset); Put(bytes,at,directory);
        Put(bytes,directory.entriesOffset,R::AssetEntry{0,directory.dataOffset,0,123,0});
    }
    header.size=bytes.size(); Put(bytes,0,header);
    TKMovesetHeader outer{}; strcpy_s(outer.version_string,"1.0"); outer.characterId=6;
    return R::Serialize(bytes,outer);
}
void Load(EditorT8Retail& editor,const std::vector<Byte>& bytes) {
    auto* copy=static_cast<Byte*>(malloc(bytes.size())); if(!copy) throw std::bad_alloc(); memcpy(copy,bytes.data(),bytes.size());
    Check(editor.LoadMoveset(copy,bytes.size()),"Retail editor load rejected fixture");
}
std::vector<Byte> Bytes(EditorT8Retail& editor) { uint64_t size; auto* data=editor.GetMoveset(size);return {data,data+size}; }
void Set(InputMap& fields,const char* key,const char* value) {auto* field=fields.at(key);strcpy_s(field->buffer,field->bufsize,value);}
struct Directory {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("retail-editor-"+std::to_string(GetCurrentProcessId()));
    Directory(){std::filesystem::create_directories(path);}
    ~Directory(){std::error_code error; std::filesystem::remove_all(path,error);}
};
}
void RunMovesetFolderTests() {
    namespace fs=std::filesystem;
    const auto previous=fs::current_path();
    const auto directory=fs::temp_directory_path()/("tkmovesets-folder-tests-"+std::to_string(GetCurrentProcessId()));
    Check(fs::create_directory(directory),"Cannot create folder settings test directory");
    fs::current_path(directory);
    Settings::SetFilePath(SETTINGS_FILE); // in the test folder, never the user's AppData settings
    try {
        const auto extra=fs::absolute(directory/fs::path(L"extra # folder \u00e9"));
        const auto output=fs::absolute(directory/"custom output");
        fs::create_directories(MOVESET_DIRECTORY);fs::create_directories(extra);fs::create_directories(output);
        auto utf8=[](const fs::path& p){return Helpers::to_utf8(p.wstring());};
        MovesetFolders::Save({});
        Check(MovesetFolders::ExtractionDirectory()==fs::path(MOVESET_DIRECTORY),"Empty extraction setting lost default");
        bool rejected=false;try{MovesetFolders::Save({"relative/folder",{}});}catch(const std::exception&){rejected=true;}
        Check(rejected,"Relative extraction folder was accepted");
        MovesetFolders::Save({utf8(output),{utf8(extra),utf8(extra/".")}});
        Check(Settings::LoadFile(),"Folder settings reload failed");MovesetFolders::Load();
        Check(MovesetFolders::Get().additional[0]==utf8(extra) && MovesetFolders::ExtractionDirectory()==output,
            "Folder settings did not survive spaces, comment characters and persistence");
        Check(MovesetFolders::SearchDirectories().size()==3,"Search folders were duplicated or default was omitted");
        struct PathExtractor:ExtractorT8 {using ExtractorT8::ExtractorT8;using ExtractorT8::GetFilepath;};
        GameProcess process;GameData game;GameInfo info{};
        info.dataString=info.minorDataString="t8-retail";info.movesetNamePrefix="T8_";
        PathExtractor extractor(process,game,&info);std::wstring full,temporary;
        extractor.GetFilepath("Test",full,temporary,false);
        Check(fs::path(full).parent_path()==output,"Extraction did not use the configured output folder");
        for(const auto& path:{fs::absolute(MOVESET_DIRECTORY)/"same.tkmvst",extra/"same.tkmvst",output/"output.tkmvst"}) {
            std::ofstream f(path,std::ios::binary);f<<"invalid fixture";
        }
        LocalStorage storage;storage.ReloadMovesetList();storage.CleanupUnusedMovesetInfos();
        Check(storage.extractedMovesets.size()==3,"Character discovery lost equal filenames from different folders");
        MovesetFolders::Save({utf8(output),{utf8(directory/"missing folder")}});
        storage.ReloadMovesetList();storage.CleanupUnusedMovesetInfos();
        Check(storage.extractedMovesets.size()==2,"Removed/missing search folder left characters in the list");
        MovesetFolders::Save({});storage.ReloadMovesetList();storage.CleanupUnusedMovesetInfos();
        Check(storage.extractedMovesets.size()==1,"Clearing custom extraction folder did not restore default discovery");
        extractor.GetFilepath("Test",full,temporary,false);
        Check(fs::weakly_canonical(fs::path(full).parent_path())==fs::weakly_canonical(MOVESET_DIRECTORY),"Cleared extraction setting did not restore default output");
        fs::remove(extra/"same.tkmvst");fs::remove(output/"output.tkmvst");fs::remove(fs::path(MOVESET_DIRECTORY)/"same.tkmvst");
        fs::remove(extra);fs::remove(output);fs::remove(MOVESET_DIRECTORY);
        Check(Settings::LoadFile(),"Cleared folder settings did not reload");MovesetFolders::Load();
        Check(MovesetFolders::Get().extraction.empty(),"Cleared extraction override was not persisted");
        fs::remove(SETTINGS_FILE);
    } catch (...) {fs::current_path(previous);Settings::SetFilePath({});throw;}
    fs::current_path(previous);fs::remove(directory);Settings::SetFilePath({});Settings::LoadFile();MovesetFolders::Load();
}

namespace {
// Native-layout body animation written by the independent encoder (same layout as game files).
std::vector<Byte> NativeLikePanm(uint32_t frames,float seed,size_t bones=22) {
    RetailPanm::Animation animation{frames,{}};
    for(size_t i=0;i<bones;++i){
        RetailPanm::Track track{i==0?"Top":i==1?"Trans":"Bone"+std::to_string(i),{}};
        track.samples.resize(i%3?1:frames);
        for(size_t f=0;f<track.samples.size();++f)track.samples[f].translation={seed,float(f),float(i)};
        animation.tracks.push_back(std::move(track));
    }
    return RetailPanm::Encode(animation);
}
// v3 snapshot: move 0 -> body instance 0x100 (a), move 1 -> 0x101 (b, or 0x100 when shared), plus a hands entry.
std::vector<Byte> PanmFixture(const std::vector<Byte>& a,const std::vector<Byte>& b,bool shared=false) {
    auto file=Fixture(false);auto* outer=reinterpret_cast<TKMovesetHeader*>(file.data());
    std::vector<Byte> payload(file.begin()+outer->moveset_data_start,file.end());
    auto header=*reinterpret_cast<R::Header*>(payload.data());header.version=3;
    auto moves=*reinterpret_cast<R::Entry*>(payload.data()+header.directoryOffset+12*sizeof(R::Entry));
    for(uint64_t row=0;row<moves.count;++row){
        Put<uint64_t>(payload,moves.offset+row*moves.stride+0x48,0x7ff000000000ULL+row);
        Put<uint64_t>(payload,moves.offset+row*moves.stride+0x50,(1ULL<<32)|(shared?0x100:0x100+row));
    }
    auto align=[](size_t n){return (n+7)&~size_t(7);};
    std::vector<std::pair<R::AssetEntry,std::vector<Byte>>> assets{{{0x7ff100000000ULL,0,a.size(),0x100,0},a},{{0x7ff200000000ULL,0,b.size(),0x101,0},b},{{0x7ff300000000ULL,0,a.size(),0x100,1},a}};
    payload.resize(align(payload.size()));const auto at=payload.size();
    R::AssetDirectory directory{{'T','8','A','N'},uint32_t(assets.size()),at+sizeof(R::AssetDirectory),align(at+sizeof(R::AssetDirectory)+assets.size()*sizeof(R::AssetEntry)),0};
    payload.resize(directory.dataOffset);Put(payload,at,directory);
    for(size_t i=0;i<assets.size();++i){payload.resize(align(payload.size()));assets[i].first.offset=payload.size();Put(payload,directory.entriesOffset+i*sizeof(R::AssetEntry),assets[i].first);payload.insert(payload.end(),assets[i].second.begin(),assets[i].second.end());}
    header.size=payload.size();Put(payload,0,header);
    TKMovesetHeader newOuter{};strcpy_s(newOuter.version_string,"1.0");newOuter.characterId=6;
    return R::Serialize(payload,newOuter);
}
std::span<const Byte> PayloadOf(const std::vector<Byte>& file){auto* outer=reinterpret_cast<const TKMovesetHeader*>(file.data());return std::span<const Byte>(file).subspan(outer->moveset_data_start);}
uint32_t MoveU32(EditorT8Retail& editor,uint32_t row,size_t at){uint32_t v;memcpy(&v,editor.Record(12,row).data()+at,4);return v;}
uint64_t MoveU64(EditorT8Retail& editor,uint32_t row,size_t at){uint64_t v;memcpy(&v,editor.Record(12,row).data()+at,8);return v;}
void RunRetailPanmTests(const GameInfo* game) {
    const auto a=NativeLikePanm(5,1),b=NativeLikePanm(9,2),c=NativeLikePanm(7,3);
    for(const auto* panm:{&a,&b,&c})EditorT8Retail::ValidatePanm(*panm);
    GameProcess process;GameData data;EditorT8Retail editor(process,data,game);
    const auto original=PanmFixture(a,b);Load(editor,original);
    // Export: single move and every distinct body animation.
    Check(editor.MoveAnimation(0)==a && editor.MoveAnimation(1)==b && editor.AnimationInstance(1)==0x101u,"PANM export differs from embedded asset");
    Directory temp;const auto exported=temp.path/"panms";
    Check(editor.ExportAnimations(exported,[](uint32_t){return std::string("idle:1");})==2,"PANM export count differs");
    auto manifest=nlohmann::json::parse(std::ifstream(exported/"animations.json"));
    Check(manifest.size()==2 && manifest[0]["moves"]==nlohmann::json::array({0}) && manifest[1]["instance"]==0x101,"PANM export manifest differs");
    const auto fileA=exported/manifest[0]["file"].get<std::string>(),fileB=exported/manifest[1]["file"].get<std::string>();
    Check(manifest[0]["file"].get<std::string>().find(':')==std::string::npos && EditorT8Retail::ReadPanmFile(fileA)==a && EditorT8Retail::ReadPanmFile(fileB)==b,"Exported PANM files differ");
    Reject([&]{editor.ExportAnimations(exported);});
    Check(Bytes(editor)==original,"PANM export changed the document");
    // Round trip of an exported native animation: rebinding, no custom asset.
    Check(editor.FindAnimationFile(fileA.c_str())==0,"Exported PANM binding not found");
    auto reused=editor.ImportPanm(1,EditorT8Retail::ReadPanmFile(fileA));
    Check(!reused.custom && MoveU64(editor,1,0x20)==MoveU64(editor,0,0x20) && MoveU64(editor,1,0x50)==MoveU64(editor,0,0x50),"Identical PANM did not reuse its binding");
    Check(R::Assets(PayloadOf(Bytes(editor))).size()==3 && editor.MoveAnimation(1)==a,"Identical PANM changed the asset directory");
    // Custom PANM on a move playing a native animation: a new instance and animation name of its own, the native
    // animation and its asset stay untouched.
    Load(editor,original);
    const auto nativeKey=MoveU64(editor,0,0x20);
    auto custom=editor.ImportPanm(0,c,"my anim");
    Check(custom.custom && !custom.replaced && custom.instance==0x10000 && custom.name=="my anim","Custom PANM import result differs");
    Check(editor.MoveAnimation(0)==c && editor.MoveAnimation(1)==b && MoveU32(editor,0,0x120)==7 && editor.AnimationInstance(0)==0x10000u &&
        (MoveU64(editor,0,0x50)>>32)==1 && MoveU64(editor,0,0x20)!=nativeKey,"Custom PANM not bound to its move");
    {
        auto hash=EditorT8Retail::Decode(MoveU64(editor,0,0x20),MoveU64(editor,0,0x28));auto* name=hash?editor.CustomName(*hash):nullptr;
        Check(name && *name=="my anim" && *hash==RetailNames::Hash("my anim"),"Custom PANM animation name differs");
        auto bytes=Bytes(editor);auto assets=R::Assets(PayloadOf(bytes));
        Check(assets.size()==4 && assets[0].address==0x7ff100000000ULL && assets[0].instance==0x100 && !memcmp(PayloadOf(bytes).data()+assets[0].offset,a.data(),a.size()) &&
            assets[1].address==0x7ff200000000ULL && assets[2].type==1 && assets[3].address==T7AnimToRetailT8::CustomAssetAddress && assets[3].instance==0x10000 && assets[3].type==0,
            "Custom PANM asset directory differs");
    }
    // Save/reload, export again and re-import: the custom asset round-trips and is reused.
    auto saved=editor.BuildSaveSnapshot();
    {
        EditorT8Retail reopened(process,data,game);Load(reopened,saved);
        Check(reopened.MoveAnimation(0)==c,"Custom PANM lost on save/reload");
        const auto again=temp.path/"again";Check(reopened.ExportAnimations(again)==2,"Custom PANM export count differs");
        auto list=nlohmann::json::parse(std::ifstream(again/"animations.json"));
        Check(list.size()==2 && list[1]["custom"]==true && list[1]["instance"]==0x10000 && EditorT8Retail::ReadPanmFile(again/list[1]["file"].get<std::string>())==c,"Custom PANM export differs");
        auto result=reopened.ImportPanm(1,EditorT8Retail::ReadPanmFile(again/list[1]["file"].get<std::string>()));
        Check(!result.custom && reopened.MoveAnimation(1)==c && MoveU64(reopened,1,0x50)==MoveU64(reopened,0,0x50),"Re-imported custom PANM was not reused");
        auto* header=reinterpret_cast<const TKMovesetHeader*>(saved.data());
        Check(RetailT8Import::Prepare(*header,PayloadOf(saved)).Ok(),"Custom PANM document no longer prepares for import");
        // Live editing must fall back to a full import, which attaches custom assets.
        EditorT8Retail next(process,data,game);Load(next,saved);next.ImportPanm(1,NativeLikePanm(6,8));auto sync=RetailLiveEdit::Sync(process,0,0,saved,next.BuildSaveSnapshot(),RetailT8Runtime::PlayerOffsets{0x3b50,0x168});// The import adds an animation name (metadata) and an asset entry: either is structural.
        if(!(sync.structural && (sync.reason==std::string_view("animation assets changed") || sync.reason==std::string_view("move names or row provenance changed"))))throw std::runtime_error("Live sync ignored a PANM import: "+std::string(sync.reason));
    }
    // The move's own custom animation, used by no other move, is replaced in place (no new entry).
    {
        auto replaced=editor.ImportPanm(0,NativeLikePanm(11,4),"my anim");
        Check(replaced.custom && replaced.replaced && replaced.instance==0x10000 && replaced.name=="my anim" && R::Assets(PayloadOf(Bytes(editor))).size()==4 &&
            MoveU32(editor,0,0x120)==11 && editor.MoveAnimation(1)==b,"Custom PANM replacement differs");
    }
    // Another move importing under the same name gets a unique name and its own instance.
    {
        auto other=editor.ImportPanm(1,NativeLikePanm(6,9),"my anim");
        Check(other.custom && !other.replaced && other.instance==0x10001 && other.name=="my anim (2)" && editor.MoveAnimation(0)==NativeLikePanm(11,4) &&
            R::Assets(PayloadOf(Bytes(editor))).size()==5,"Second custom PANM under the same name differs");
    }
    // Rejections leave the document untouched.
    Load(editor,original);
    auto canonical=RetailPanm::Encode(RetailPanm::DecodeRaw(c));
    auto truncated=c;truncated.resize(truncated.size()/2);
    auto garbage=c;memset(garbage.data()+8,0xff,64);
    for(const auto* bad:{&truncated,&garbage}){Reject([&]{editor.ImportPanm(0,*bad);});Reject([&]{EditorT8Retail::ValidatePanm(*bad);});}
    Check(canonical==c,"Encoder output is not stable");Reject([&]{EditorT8Retail::ValidatePanm(NativeLikePanm(4,5,12));});
    Reject([&]{editor.ImportPanm(2,c);});
    Reject([&]{EditorT8Retail::ReadPanmFile(temp.path/"missing.panm");});
    Check(Bytes(editor)==original,"Rejected PANM import changed the document");
    // A partial skeleton is rewritten over an embedded native animation (missing bones keep its pose).
    for(const auto& foreign:{NativeLikePanm(4,5,12)}){
        Load(editor,original);auto result=editor.ImportPanm(0,foreign);
        auto stored=editor.MoveAnimation(0);EditorT8Retail::ValidatePanm(stored);
        auto source=RetailPanm::DecodeRaw(foreign),written=RetailPanm::DecodeRaw(stored);
        Check(result.custom && result.rewritten && stored!=foreign && written.frames==source.frames && written.tracks.size()==22,"Foreign PANM rewrite differs");
        for(const auto& track:source.tracks){auto at=std::find_if(written.tracks.begin(),written.tracks.end(),[&](const auto& t){return t.name==track.name;});
            Check(at!=written.tracks.end() && at->samples.size()==track.samples.size() && !memcmp(at->samples.data(),track.samples.data(),track.samples.size()*sizeof(RetailPanm::Sample)),"Rewritten PANM samples differ");}
    }
    Check(!editor.ImportPanm(0,c).rewritten,"Native-layout PANM was rewritten");
    Load(editor,original);
    // A failed batch rolls the custom asset back.
    try{editor.MutateBatch([&]{editor.ImportPanm(0,c);throw std::runtime_error("abort");});}catch(const std::runtime_error&){}
    Check(Bytes(editor)==original,"Aborted PANM import was not rolled back");
    // Shared animation: the move leaves it for a new instance, the other move keeps playing it.
    const auto sharedFile=PanmFixture(a,b,true);Load(editor,sharedFile);
    {
        const auto sharedKey=MoveU64(editor,1,0x20),sharedHandle=MoveU64(editor,1,0x50);
        auto own=editor.ImportPanm(0,c);
        Check(own.custom && !own.replaced && own.name=="custom animation" && editor.MoveAnimation(0)==c && editor.MoveAnimation(1)==a &&
            MoveU64(editor,1,0x20)==sharedKey && MoveU64(editor,1,0x50)==sharedHandle && MoveU64(editor,0,0x20)!=sharedKey,"Shared PANM import changed the other move");
    }
    // Converted documents: placeholder handles are repaired from the donor's move table by animation hash.
    {
        auto file=Fixture(false);auto* outer=reinterpret_cast<TKMovesetHeader*>(file.data());
        const auto payloadAt=outer->moveset_data_start;
        auto header=*reinterpret_cast<R::Header*>(file.data()+payloadAt);
        auto moves=*reinterpret_cast<R::Entry*>(file.data()+payloadAt+header.directoryOffset+12*sizeof(R::Entry));
        std::vector<Byte> donor(file.begin()+payloadAt+moves.offset,file.begin()+payloadAt+moves.offset+moves.count*moves.stride);
        for(uint64_t row=0;row<moves.count;++row){
            Put<uint64_t>(file,payloadAt+moves.offset+row*moves.stride+0x50,1);
            Put<uint64_t>(donor,row*moves.stride+0x50,0xef00060000000200ULL+row);
        }
        std::vector<Byte> payload(file.begin()+payloadAt,file.end());TKMovesetHeader plain{};strcpy_s(plain.version_string,"1.0");plain.characterId=6;
        const auto converted=R::Serialize(payload,plain);Load(editor,converted);
        Reject([&]{editor.ImportPanm(0,c);});
        Check(editor.AdoptAnimationHandles(donor)==2 && editor.AnimationInstance(1)==0x201u,"Converted handles were not repaired");
        Check(editor.AdoptAnimationHandles(donor)==0,"Repaired handles were rewritten");
        Reject([&]{editor.AdoptAnimationHandles(std::span<const Byte>(donor).first(10));});
        // A tables-only document gains an animation directory holding the new custom entry.
        auto result=editor.ImportPanm(0,c);
        Check(result.custom && result.instance==0x10000 && editor.MoveAnimation(0)==c && (MoveU64(editor,0,0x50)>>32)==0xef000600u && editor.AnimationInstance(1)==0x201u,
            "New custom entry import differs");
        auto bytes=editor.BuildSaveSnapshot();auto* saved=reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        auto assets=R::Assets(PayloadOf(bytes));
        Check(saved->game_specific_flags==R::RuntimeAssetsFlag && assets.size()==1 && assets[0].address==T7AnimToRetailT8::CustomAssetAddress && assets[0].instance==0x10000,
            "Tables-only document did not gain a custom animation directory");
        Check(RetailT8Import::Prepare(*saved,PayloadOf(bytes)).Ok(),"Upgraded converted document no longer prepares for import");
        EditorT8Retail reopened(process,data,game);Load(reopened,bytes);
        auto second=reopened.ImportPanm(1,b);Check(second.custom && R::Assets(PayloadOf(reopened.BuildSaveSnapshot())).size()==2,"Second new custom entry differs");
    }
    // Documents whose moves have no resolved handle cannot store custom PANMs.
    Load(editor,Fixture(false));Reject([&]{editor.ImportPanm(0,c);});Reject([&]{editor.MoveAnimation(0);});
    Load(editor,Fixture());Reject([&]{editor.ImportPanm(0,c);});
}
}
// Retail command constants differ from T7 (0x8012 group, 0x8013 group end, 0x8014+n input sequence).
void RunRetailCommandTests(const GameInfo* game) {
    namespace C=RetailT8Commands;
    // Retail buttons: bit 4 Heat, bit 6 Rage Art, bit 29 = any one of the buttons (RE findings log 2026-11-14).
    Check(C::CommandString(0x4000000100000040ULL)=="F+1" && C::CommandString(0x20)=="N" && C::CommandString(0x2000000c00000000ULL)=="3|4" &&
        C::CommandString(0x248)=="D/F|F|U/F" && C::CommandString(0x0000001000000000ULL)=="HE" && C::CommandString(0x0000004000000000ULL)=="RA",
        "Retail command decoding differs from the verified button bits");
    Check(C::CommandString(C::ListEnd)=="AUTO" && C::CommandString(C::GroupCancel)=="GROUP" && C::CommandString(C::GroupEnd)=="END" &&
        C::CommandString(0x800d)=="0x800D" && C::CommandString(0x8015).empty(),"Retail special command text differs");
    Check(!C::IsInputSequence(0x800d) && !C::IsInputSequence(C::GroupCancel) && C::IsInputSequence(0x8014) && C::InputSequenceId(0x8016)==2,"Retail input sequence range differs");
    auto bytes=Fixture(true,[](std::vector<Byte>& b,size_t table,const R::Entry& entry){
        if(table==6){
            Put<uint64_t>(b,entry.offset,C::InputSequenceBase+1);Put<uint16_t>(b,entry.offset+36,1);                    // cancel 0 -> input sequence 1, move 1
            Put<uint64_t>(b,entry.offset+entry.stride,C::GroupCancel);Put<uint16_t>(b,entry.offset+entry.stride+36,0); // cancel 1 -> group list 0
        }
        if(table==7)Put<uint64_t>(b,entry.offset,C::GroupEnd);
        if(table==14){Put<uint16_t>(b,entry.offset+entry.stride+2,2);Put<uint64_t>(b,entry.offset+entry.stride+8,0x100000+15*0x100000);}
        if(table==15){Put<uint64_t>(b,entry.offset,0x4000000100000040ULL);Put<uint64_t>(b,entry.offset+entry.stride,0x8);}
    });
    GameProcess process;GameData data;EditorT8Retail editor(process,data,game);Load(editor,bytes);
    Check(editor.InputSequenceText(1)=="F+1, D/F" && editor.InputSequenceText(0)=="0 inputs","Retail input sequence text differs");
    auto refers=[&](size_t table,uint32_t row,size_t source,uint32_t id){auto list=editor.ReferencesTo(table,row);return std::any_of(list.begin(),list.end(),[&](const auto& r){return r.table==source && r.row==id;});};
    Check(refers(14,1,6,0) && !refers(14,1,6,1),"Input sequence reference uses the T7 base");
    Check(refers(7,0,6,1) && !refers(12,0,6,1),"Grouped cancel reference uses the T7 command");
    Check(editor.ListSize(7,0)==1,"Group cancel list does not end at 0x8013");
    auto created=editor.CreateNew(EditorT8Retail::Type(7));
    Check(editor.Record(7,created+editor.ListSize(7,created)-1).size()==R::Tables[7].stride &&
        *reinterpret_cast<const uint64_t*>(editor.Record(7,created+editor.ListSize(7,created)-1).data())==C::GroupEnd,"New group cancel list is not terminated with 0x8013");
}
void RunRetailEditorTests() {
    RunMovesetFolderTests();
    auto* game=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);
    Check(game && game->IsEditable() && Games::IsGameLiveEditable(GameId_T8,GameVersions::T8::RETAIL),"Retail editor/live registration differs");
    RunRetailPanmTests(game);
    RunRetailCommandTests(game);
    for(uint64_t key:{0ULL,1ULL,0x123456789abcdef0ULL,UINT64_MAX})
        for(uint32_t value:{0U,1U,31U,32U,0x80000000U,UINT32_MAX}) Check(EditorT8Retail::Decode(EditorT8Retail::Encode(value,key),key)==value,"Retail encoded field roundtrip failed");
    Check(!EditorT8Retail::Decode(0,0),"Invalid encrypted checksum accepted");
    GameProcess process; GameData data; EditorT8Retail editor(process,data,game); auto original=Fixture(); Load(editor,original);
    Check(Bytes(editor)==original,"Retail load altered bytes");
    Check(editor.ResolveMove(0x8001)==0 && editor.ResolveMove(1)==1 && !editor.MoveAliases(0).empty(),"Retail jump-by-ID/generic alias mapping differs");
    Reject([&]{editor.ResolveMove(UINT32_MAX);});
    auto names=std::vector<Byte>();for(uint32_t row=0;row<editor.Count(12);++row){auto record=editor.Record(12,row);names.insert(names.end(),record.begin(),record.end());}
    Check(editor.MatchesMoveNames(names),"Retail current-move matching rejected the document's move identities");
    names[0]^=1;Check(!editor.MatchesMoveNames(names),"Retail current-move matching accepted another move identity");
    const auto references=editor.ReferencesTo(12,0);
    Check(std::any_of(references.begin(),references.end(),[](const auto& r){return r.table==6 && r.field=="move_id";}) &&
        std::any_of(references.begin(),references.end(),[](const auto& r){return r.table==12 && r.field=="transition";}) &&
        std::any_of(references.begin(),references.end(),[](const auto& r){return r.table==0 && r.field=="standing_moveid";}),"Retail reference search omitted cancel/transition/reaction move IDs");
    {
        VectorSet<std::string> order;auto fields=editor.RecordFields(0,1,order);Set(fields,"standing_moveid","32769");editor.SaveRecord(0,1,fields);
        auto found=editor.ReferencesTo(12,0);
        Check(std::any_of(found.begin(),found.end(),[](const auto& r){return r.table==0 && r.row==1 && r.field=="standing_moveid";}),"Retail reference search omitted reaction aliases");
        Load(editor,original);
        order.clear();fields=editor.RecordFields(4,0,order);Set(fields,"extra","65536");editor.SaveRecord(4,0,fields);
        found=editor.ReferencesTo(5,65536);
        Check(std::any_of(found.begin(),found.end(),[](const auto& r){return r.table==4 && r.row==0 && r.field=="extra";}),"Retail reference search truncated target row IDs");
        Load(editor,original);
    }
    for(size_t table=0;table<20;++table) {
        VectorSet<std::string> order; auto fields=editor.RecordFields(table,0,order);
        uint64_t size;auto* stable=editor.GetMoveset(size); editor.SaveRecord(table,0,fields);
        Check(editor.GetMoveset(size)==stable,"Scalar edit unnecessarily replaced full allocation");
        Check(Bytes(editor)==original,"No-op retail field save changed opaque bytes");
    }
    {
        // Layout verified in the game (RE project findings log 2026-11-14: reaction picker 0x1418FE330,
        // hit-line builder 0x1418C3EE0, hitbox live test 0x141913960).
        auto at=[](size_t table,const char* name)->std::optional<std::pair<uint32_t,EditorT8Retail::Kind>>{
            for(const auto& f:EditorT8Retail::Schema(table))if(f.name==name)return std::pair{f.offset,f.kind};return std::nullopt;};
        using K=EditorT8Retail::Kind;
        Check(at(0,"downed_rotation")==std::pair{0x4eU,K::U16} && at(0,"standing_moveid")==std::pair{0x50U,K::U16} &&
            at(0,"downed_moveid")==std::pair{0x6aU,K::U16} && !at(0,"default_moveid") && at(4,"extra_count")==std::pair{4U,K::U32} &&
            at(12,"damage_override")==std::pair{0x118U,K::S16} && at(12,"active_start")==std::pair{0x158U,K::U32} &&
            at(12,"hitbox_0_radius")==std::pair{0x16cU,K::F32} && at(12,"hitbox_0_offset_b_2")==std::pair{0x188U,K::F32} &&
            at(12,"hitbox_0_starts_off")==std::pair{0x18cU,K::U32} && at(12,"hitbox_7_starts_off")==std::pair{0x2dcU,K::U32},
            "Retail schema differs from the verified reaction/hitbox layout");
    }
    {
        VectorSet<std::string> order; auto fields=editor.RecordFields(12,0,order);
        Check(fields.at("cancels")->category==2 && fields.at("cancels_2")->category==3 &&
            fields.at("hitbox_0_start")->category==7 && fields.at("hitbox_7_start")->category==21,
            "Retail move categories lost references, extras or individual collapsed hitboxes");
        Check(fields.at("active_start")->color==AppTheme::Input && fields.at("cancels")->color==FORM_INPUT_REF &&
            fields.at("hit_level")->color==FORM_INPUT_HEX,"Retail form colours differ from established editor palette");
        Set(fields,"transition","70000");Check(!editor.ValidateField(EditorT8Retail::Type(12),fields.at("transition")),"Typing validation accepted overflowing U16");
        Set(fields,"transition","0");
        Set(fields,"active_start","12"); Set(fields,"vulnerability","4294967295"); Set(fields,"hit_level","7");
        auto before=Bytes(editor); editor.SaveRecord(12,0,fields);
        Check(EditorT8Retail::Decode(*reinterpret_cast<const uint64_t*>(editor.Record(12,0).data()+0x58),*reinterpret_cast<const uint64_t*>(editor.Record(12,0).data()+0x60))==UINT32_MAX,"Retail encoded edit did not decode");
        VectorSet<std::string> fresh; auto edited=editor.RecordFields(12,0,fresh);
        Check(std::string(edited.at("active_start")->buffer)=="12","Retail move edit was not retained");
        auto saved=Bytes(editor); Set(edited,"active_start","999"); Set(edited,"transition","70000");
        Reject([&]{editor.SaveRecord(12,0,edited);}); Check(Bytes(editor)==saved,"Invalid late field partially changed retail record");
        Set(edited,"transition","0"); Set(edited,"name_hash","99"); Reject([&]{editor.SaveRecord(12,0,edited);});
        Check(Bytes(editor)==saved,"Read-only name hash changed");
    }
    {
        auto floats=original; auto* outer=reinterpret_cast<TKMovesetHeader*>(floats.data());
        R::Header header;memcpy(&header,floats.data()+outer->moveset_data_start,sizeof(header));
        R::Entry moves;memcpy(&moves,floats.data()+outer->moveset_data_start+header.directoryOffset+12*sizeof(moves),sizeof(moves));
        const size_t offset=outer->moveset_data_start+moves.offset;
        for (uint32_t bits:{1U,0x80000000U,0x3f800001U,0x7fc12345U,0x7f800000U}) {
            Put(floats,offset+0x16c,bits);outer->crc32=Helpers::CalculateCrc32(floats.data()+outer->moveset_data_start,floats.size()-outer->moveset_data_start);
            Load(editor,floats);VectorSet<std::string> order;auto fields=editor.RecordFields(12,0,order);editor.SaveRecord(12,0,fields);
            Check(Bytes(editor)==floats,"Float no-op lost original bits");
            Set(fields,"hitbox_0_radius","nan");auto before=Bytes(editor);Reject([&]{editor.SaveRecord(12,0,fields);});Check(Bytes(editor)==before,"Invalid float changed record");
        }
        auto external=original;auto* extHeader=reinterpret_cast<TKMovesetHeader*>(external.data());
        R::Entry sequences;memcpy(&sequences,external.data()+extHeader->moveset_data_start+header.directoryOffset+14*sizeof(sequences),sizeof(sequences));
        Put<uint16_t>(external,extHeader->moveset_data_start+sequences.offset+2,1);
        Put<uint64_t>(external,extHeader->moveset_data_start+sequences.offset+8,0xdeadbeef);
        extHeader->crc32=Helpers::CalculateCrc32(external.data()+extHeader->moveset_data_start,external.size()-extHeader->moveset_data_start);
        Load(editor,external);VectorSet<std::string> order;auto fields=editor.RecordFields(14,0,order);editor.SaveRecord(14,0,fields);Check(Bytes(editor)==external,"External input reference was not preserved");
        Set(fields,"inputs","-1");Reject([&]{editor.SaveRecord(14,0,fields);});Check(Bytes(editor)==external,"Read-only external pointer changed");
    }
    Load(editor,original);
    {
        VectorSet<std::string> order; auto fields=editor.RecordFields(6,0,order);
        Set(fields,"requirements","1"); Set(fields,"move_id","1"); editor.SaveRecord(6,0,fields);
        auto saved=Bytes(editor); Set(fields,"requirements","2"); Reject([&]{editor.SaveRecord(6,0,fields);}); Check(Bytes(editor)==saved,"Invalid target partially changed cancel");
        Set(fields,"requirements","-1"); editor.SaveRecord(6,0,fields);
    }
    {
        VectorSet<std::string> order; auto fields=editor.RecordFields(1,1,order);
        auto saved=Bytes(editor); Set(fields,"condition","12"); Reject([&]{editor.SaveRecord(1,1,fields);}); Check(Bytes(editor)==saved,"List terminator changed");
    }
    {
        VectorSet<std::string> order; auto fields=editor.RecordFields(14,0,order);
        Set(fields,"input_count","2");Set(fields,"inputs","1"); auto saved=Bytes(editor);
        Reject([&]{editor.SaveRecord(14,0,fields);});Check(Bytes(editor)==saved,"Overrunning input range changed bytes");
    }
    {
        VectorSet<std::string> order; auto fields=editor.RecordFields(5,65536,order);Set(fields,"value","123");editor.SaveRecord(5,65536,fields);
        Check(*reinterpret_cast<const uint16_t*>(editor.Record(5,65536).data())==123,"32-bit retail row ID was truncated");
    }
    {
        auto before=Bytes(editor); uint64_t size; const auto* pointer=editor.GetMoveset(size);
        try { EditorLogic::Mutation transaction(editor,true); VectorSet<std::string> order; auto fields=editor.RecordFields(8,0,order);Set(fields,"value","44");editor.SaveRecord(8,0,fields);throw std::runtime_error("callback"); } catch(const std::exception&) {}
        Check(Bytes(editor)==before && editor.GetMoveset(size)==pointer,"Retail callback rollback lost bytes/pointer");
    }
    auto bytes=Bytes(editor); auto crc=editor.CalculateCRC32(); auto snapshot=editor.BuildSaveSnapshot();
    Check(Bytes(editor)==bytes && reinterpret_cast<const TKMovesetHeader*>(snapshot.data())->crc32==crc,"Retail save snapshot changed active data or CRC");
    Directory directory;
    for(auto codec:{TKMovesetCompressionType_None,TKMovesetCompressionType_LZ4,TKMovesetCompressionType_LZMA}) {
        auto path=directory.path/(std::to_string(codec)+".tkmvst"); FileJobRequest request;request.output=path;request.bytes=snapshot;request.codec=codec;
        auto result=RunFileJob(std::move(request));Check(result.ok,"Retail save job failed");
        MovesetFile reloaded(path);reloaded.Validate();Check(std::equal(reloaded.Payload().begin(),reloaded.Payload().end(),snapshot.begin()+reinterpret_cast<const TKMovesetHeader*>(snapshot.data())->moveset_data_start),"Retail compressed roundtrip changed payload/assets");
    }
    auto malformed=original; malformed.back()^=1; auto before=Bytes(editor);
    auto* bad=static_cast<Byte*>(malloc(malformed.size()));memcpy(bad,malformed.data(),malformed.size());Check(!editor.LoadMoveset(bad,malformed.size()) && Bytes(editor)==before,"Failed retail reload changed active moveset");
    Load(editor,original);
    for(size_t table=0;table<20;++table){
        Load(editor,original);const auto count=editor.Count(table);
        auto id=editor.CreateNew(EditorT8Retail::Type(table));Check(id==count && editor.Count(table)>count,"Create new did not append retail records");
        auto snapshot=editor.BuildSaveSnapshot();EditorT8Retail reopened(process,data,game);Load(reopened,snapshot);
        Check(reopened.Count(table)==editor.Count(table),"Created retail rows did not reload");
        if(table==12){
            Check(editor.CustomName(*editor.MoveNameHash(id)) && editor.CustomName(*editor.MoveNameHash(id))->starts_with("tkm_"),"New move name was not persisted");
            // Defaults: a copy of the idle stance with its own one-row AUTO-to-idle cancel list and empty lists.
            VectorSet<std::string> order;auto fields=editor.RecordFields(12,id,order);
            const auto cancels=std::stoul(fields.at("cancels")->buffer);
            Check(cancels<editor.Count(6) && editor.ListSize(6,cancels)==1,"New move did not get its own one-row cancel list");
            uint64_t command;uint16_t target;memcpy(&command,editor.Record(6,cancels).data(),8);memcpy(&target,editor.Record(6,cancels).data()+36,2);
            Check(command==RetailT8Commands::ListEnd && target==0x8001,"New move cancel list is not AUTO to idle");
            Check(std::string(fields.at("transition")->buffer)=="32769","New move transition is not the idle alias");
            for(const char* key:{"cancels_2","cancels_3","cancels_4","voiceclips","properties","start_properties","end_properties"})
                Check(std::string(fields.at(key)->buffer)=="-1",key);
            Check(editor.MoveAliases(id).empty() && editor.MoveNameHash(id)!=editor.MoveNameHash(editor.ResolveMove(0x8001)),"New move kept the idle identity");
        }
    }
    Load(editor,original);
    auto clone=editor.DuplicateStructure(EditorT8Retail::Type(12),1,1);Check(clone==2 && editor.MoveNameHash(clone)==editor.MoveNameHash(1),"Move clone lost identity");
    editor.RenameMove(clone,"CUSTOM_Test");auto customHash=editor.MoveNameHash(clone);Check(customHash && *editor.CustomName(*customHash)=="CUSTOM_Test","Move rename failed");
    editor.SetAnimation(clone,0);
    auto beforeStructure=Bytes(editor);uint64_t beforeSize;auto beforePointer=editor.GetMoveset(beforeSize);
    Reject([&]{editor.MutateBatch([&]{editor.CreateNew(EditorT8Retail::Type(6));editor.DeleteStructures(EditorT8Retail::Type(12),999999,1);});});
    Check(Bytes(editor)==beforeStructure && editor.GetMoveset(beforeSize)==beforePointer && *editor.CustomName(*customHash)=="CUSTOM_Test","Structural batch rollback lost file or metadata");
    editor.EditRows(12,0,{2,1,0},{});Check(editor.MoveNameHash(0)==customHash && editor.ResolveMove(0x8001)==2,"Move reorder broke name or list-head alias");
    Reject([&]{editor.DeleteStructures(EditorT8Retail::Type(12),2,1);});auto removable=editor.DuplicateStructure(EditorT8Retail::Type(12),0,1);editor.DeleteStructures(EditorT8Retail::Type(12),removable,1);Check(editor.Count(12)==3,"Custom move deletion failed");
    auto structural=editor.BuildSaveSnapshot();
    for(auto codec:{TKMovesetCompressionType_None,TKMovesetCompressionType_LZ4,TKMovesetCompressionType_LZMA}){
        auto path=directory.path/("structural-"+std::to_string(codec)+".tkmvst");FileJobRequest request;request.output=path;request.bytes=structural;request.codec=codec;
        Check(RunFileJob(std::move(request)).ok,"Structural retail save failed");MovesetFile file(path);file.Validate();EditorT8Retail reopened(process,data,game);Load(reopened,std::vector<Byte>(file.Bytes().begin(),file.Bytes().end()));
        Check(reopened.Count(12)==3 && reopened.MoveNameHash(0)==customHash && *reopened.CustomName(*customHash)=="CUSTOM_Test","Structural metadata failed compressed reload");
    }
    Load(editor,original);
    {
        VectorSet<std::string> order;auto fields=editor.RecordFields(14,0,order);Set(fields,"inputs","0");Set(fields,"input_count","2");editor.SaveRecord(14,0,fields);
        editor.EditRows(15,1,{-1},{});order.clear();auto after=editor.RecordFields(14,0,order);Check(std::string(after.at("input_count")->buffer)=="3","Input sequence span was not expanded");
        auto before=Bytes(editor);Reject([&]{editor.EditRows(15,0,{0,2,1},{});editor.EditRows(15,0,{0,0,1},{});});
    }
    Load(editor,original);
    editor.live_loadedMoveset=1234;editor.Live_OnFieldEdit(EditorT8Retail::Type(12),0,nullptr);Check(editor.live_loadedMoveset==0,"Retail live path retained a binding");
    {
        ImGuiContext* context=ImGui::CreateContext();AppTheme::Apply(); auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1400,900);io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
        const auto packed=[](ImGuiCol index){return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[index]);};
        Check(packed(ImGuiCol_WindowBg)==AppTheme::Background && packed(ImGuiCol_ChildBg)==AppTheme::Background && packed(ImGuiCol_DockingEmptyBg)==AppTheme::Background && packed(ImGuiCol_FrameBg)==AppTheme::Input,"Shared screenshot palette lost its backgrounds/input colour");
        unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);ImGui::NewFrame();
        movesetInfo info{}; info.filename=(directory.path/"0.tkmvst").wstring();info.name="Retail test";
        { EditorVisualsT8Retail view(&info,nullptr,nullptr), other(&info,nullptr,nullptr);
            Check(view.RecordWindowId(12,0)!=other.RecordWindowId(12,0),"Two documents share a record window ID");
            Check(EditorVisualsT8Retail::NormalizeSearch("HeLLo 0xABC")=="hello 0xabc","Search normalization differs");
            Check(view.SearchMetadata(12,0).find("0x00000000")!=std::string::npos,"Search metadata omitted move hash");
            view.OpenRecord(12,0);view.OpenRecord(5,65536);other.OpenRecord(12,0);view.Render(700);other.Render(701);
            ImGui::Render();
            ImGui::NewFrame();view.OpenRecord(0,0);view.OpenRecord(6,0);view.Render(700);ImGui::Render();
            for(int frame=0;frame<2;++frame){ImGui::NewFrame();view.Render(700);ImGui::Render();}
            for(unsigned table:{0U,6U,12U}) {
                auto* tab=ImGui::FindWindowByName(view.RecordWindowId(table,0).c_str());
                // ImGui stores zero for unmodified dock colours: move tabs use
                // the shared blue rather than a per-window override.
                const auto expected=AppTheme::RecordTab(table);
                if(!tab || tab->DockStyle.Colors[ImGuiWindowDockStyleCol_TabSelected]!=expected)throw std::runtime_error(std::format("Record tab {} colour {} differs from {}",table,tab?tab->DockStyle.Colors[ImGuiWindowDockStyleCol_TabSelected]:0,expected));
            }
            Check(packed(ImGuiCol_WindowBg)==AppTheme::Background && packed(ImGuiCol_FrameBg)==AppTheme::Input && packed(ImGuiCol_Header)==IM_COL32(30,57,87,255) && packed(ImGuiCol_TabSelected)==AppTheme::Reference,"Tab tint leaked into contents or another tab");
            view.FindReferences(12,0);
            for(int frame=0;frame<3;++frame){ImGui::NewFrame();view.Render(700);ImGui::Render();}
            auto id="###retail_references_"+std::to_string(reinterpret_cast<uintptr_t>(&view));
            auto* window=ImGui::FindWindowByName(id.c_str());
            Check(window && window->Active && window->DockId && window->DockNode && window->DockNode->SelectedTabId==window->TabId,"Find references did not reveal and select its docked results");
            ImGui::NewFrame();view.OpenRecord(12,0);view.Render(700);ImGui::Render();
            view.FindReferences(12,0);
            for(int frame=0;frame<2;++frame){ImGui::NewFrame();view.Render(700);ImGui::Render();}
            Check(window->DockNode->SelectedTabId==window->TabId,"Reopening Find references left results hidden behind another tab");
            {
                // Rows after the last terminator of a list table have no list bound (native Kuma start_properties row
                // 405). Opening one from the browser context menu, a reference result or Alt+arrows used to throw out of
                // render code and end the program; it must only report the reason.
                const auto unterminated=directory.path/"unterminated.tkmvst";
                Check(FileIO::WriteAtomically(unterminated,Fixture(true,[](std::vector<Byte>& bytes,size_t table,const R::Entry& entry){if(table==10)Put<uint32_t>(bytes,entry.offset+entry.stride+8,0);})),"Cannot write the unterminated-list document");
                movesetInfo tail{};tail.filename=unterminated.wstring();tail.name="Unterminated";
                EditorVisualsT8Retail tailView(&tail,nullptr,nullptr);
                ImGui::NewFrame();
                Check(tailView.Editor()!=nullptr,"Unterminated-list document did not load");
                Check(!tailView.OpenRecord(10,0) && tailView.StatusMessage().find("terminator")!=std::string::npos && !tailView.RecordForm(10,0),"Unbounded list row threw or opened a form");
                Check(!tailView.OpenRecord(10,2) && !tailView.OpenRecord(25,0),"Out-of-table record did not report failure");
                Check(tailView.OpenRecord(12,0) && tailView.RecordForm(12,0),"Valid record no longer opens after a failed one");
                tailView.Render(702);ImGui::Render();
            }
            ImGui::NewFrame();
        }
        ImGui::Render();ImGui::DestroyContext(context);
    }
    Load(editor,original);
    {
        VectorSet<std::string> order;auto fields=editor.RecordFields(1,0,order);auto* value=fields.at("parameter_1");
        value->flags=EditorInput_S32_Changeable;Set(fields,"parameter_1","-1");Check(editor.ValidateField(EditorT8Retail::Type(1),value),"Signed retail parameter rejected");editor.SaveRecord(1,0,fields);
        uint32_t bits;memcpy(&bits,editor.Record(1,0).data()+4,4);Check(bits==UINT32_MAX,"Signed parameter lost raw bits");
        value->flags=EditorInput_Float|EditorInput_DataChangeable;Set(fields,"parameter_1","1.5");editor.SaveRecord(1,0,fields);memcpy(&bits,editor.Record(1,0).data()+4,4);Check(bits==0x3fc00000,"Float parameter interpretation failed");
        value->flags=EditorInput_H32_Changeable;Set(fields,"parameter_1","FFFFFFFF");editor.SaveRecord(1,0,fields);memcpy(&bits,editor.Record(1,0).data()+4,4);Check(bits==UINT32_MAX,"Hex parameter interpretation failed");
        Set(fields,"parameter_1","100000000");Reject([&]{editor.SaveRecord(1,0,fields);});
        auto wide=editor.DuplicateStructure(EditorT8Retail::Type(5),65536,1);Check(wide==65537,"Wide retail duplication truncated ID");
        order.clear();auto rows=editor.GetFormFieldsListWide(EditorT8Retail::Type(5),wide,order,1);Set(rows[0],"value","345");editor.SaveItemWide(EditorT8Retail::Type(5),wide,rows[0]);uint16_t actual;memcpy(&actual,editor.Record(5,wide).data(),2);Check(actual==345,"Wide retail list save truncated ID");
    }
    Load(editor,original);
    {
        // Extra property lists end on property id 0 (+16), whatever the frame, as in the converters. A new list is its
        // {frame 0, id 0} terminator; rows are inserted before it the way the list form's Apply does it
        // (ModifyListSize, then SaveItemWide for every row): the inserted zero row is not a terminator to retain.
        auto created=editor.CreateNew(EditorT8Retail::Type(9));Check(editor.ListSize(9,created)==1,"New property list is not a lone terminator");
        editor.ModifyListSize(EditorT8Retail::Type(9),created,{-1,int(created)},{});
        VectorSet<std::string> order;auto rows=editor.GetFormFieldsListWide(EditorT8Retail::Type(9),created,order,2);
        Set(rows[0],"frame","32769");Set(rows[0],"property","33403");Set(rows[0],"parameter_1","1");
        for(uint32_t i=0;i<2;++i)editor.SaveItemWide(EditorT8Retail::Type(9),created+i,rows[i]); // rows own their fields (InputMap)
        Check(editor.ListSize(9,created)==2,"Inserted extra property row was not kept before the terminator");
        order.clear();auto fields=editor.RecordFields(9,created,order);Set(fields,"frame","0");editor.SaveRecord(9,created,fields);
        Check(editor.ListSize(9,created)==2,"A frame-0 extra property with an id ended its list");
        {order.clear();auto terminator=editor.RecordFields(9,created+1,order);Set(terminator,"property","5");auto saved=Bytes(editor);
         Reject([&]{editor.SaveRecord(9,created+1,terminator);});Check(Bytes(editor)==saved && editor.ListSize(9,created)==2,"Extra property terminator id changed");}
        Check(editor.ListSize(9,0)==2,"Fixture extra property list length differs");
        {order.clear();auto fifth=editor.RecordFields(9,created,order);Check(fifth.contains("parameter_5"),"Retail property form hides the fifth parameter");Set(fifth,"parameter_5","77");editor.SaveRecord(9,created,fifth);uint32_t value;memcpy(&value,editor.Record(9,created).data()+36,4);Check(value==77,"Fifth retail property parameter was not saved");} // S8::ExtraMoveProperty::params[4] at +36
        auto refs=editor.ReferencesTo(3,1);Check(std::any_of(refs.begin(),refs.end(),[&](auto r){return r.table==9 && r.row==created;}),"Projectile property reference omitted");
        editor.DeleteStructures(EditorT8Retail::Type(3),0,1);order.clear();auto changed=editor.RecordFields(9,created,order);Check(std::string(changed.at("parameter_1")->buffer)=="0","Projectile property ID was not relocated");
        auto snapshot=editor.BuildSaveSnapshot();snapshot[(sizeof(TKMovesetHeader)+7)&~size_t(7)]^=1;auto before=Bytes(editor);auto* bad=static_cast<Byte*>(malloc(snapshot.size()));memcpy(bad,snapshot.data(),snapshot.size());Check(!editor.LoadMoveset(bad,snapshot.size()) && Bytes(editor)==before,"Malformed structural metadata changed active editor");
    }
    Load(editor,original);
    {
        // Ctrl+B (EditorUtils::ChangeFieldDataType) changes how a value is shown, never the value stored on Apply.
        VectorSet<std::string> order;auto fields=editor.RecordFields(12,0,order);
        auto* radius=fields.at("hitbox_0_radius");Check(!(radius->flags&EditorInput_DataChangeable),"Retail float field is type-changeable");
        Set(fields,"hitbox_0_radius","1");radius->flags|=EditorInput_DataChangeable;const auto floatFlags=radius->flags;EditorUtils::ChangeFieldDataType(radius);
        Check(radius->flags==floatFlags && !radius->nextValue,"Cycling a float field changed its interpretation");
        auto cycle=[](EditorInput* field){EditorUtils::ChangeFieldDataType(field);field->ConsumeNextValue();return std::string(field->buffer);};
        auto* damage=fields.at("damage_override");Set(fields,"damage_override","-5");
        Check(cycle(damage)=="0xFFFB","Negative S16 shown as hex outside 16 bits");
        editor.SaveRecord(12,0,fields);int16_t stored;memcpy(&stored,editor.Record(12,0).data()+0x118,2);Check(stored==-5,"Hex S16 view changed the stored value");
        Check(cycle(damage)=="65531" && cycle(damage)=="-5","S16 display cycle changed the value");
        editor.SaveRecord(12,0,fields);memcpy(&stored,editor.Record(12,0).data()+0x118,2);Check(stored==-5,"S16 display cycle changed the stored value");
        Set(fields,"active_start","4294967295");Check(cycle(fields.at("active_start"))=="-1" && cycle(fields.at("active_start"))=="0xFFFFFFFF" && cycle(fields.at("active_start"))=="4294967295","U32 display cycle changed the value");
        order.clear();auto cancel=editor.RecordFields(6,0,order);auto* command=cancel.at("command");Set(cancel,"command","0x8000000000000001");
        Check(cycle(command)=="9223372036854775809" && cycle(command)=="-9223372036854775807" && cycle(command)=="0x8000000000000001","64-bit display cycle saturated a value >= 2^63");
    }
    {
        // Encrypted move fields: {value, key} then four u32 slots holding 0x765 + row, except slot row % 8 - 4 which keeps
        // the plain value when row % 8 >= 4 (docs/conversion.md). On row 5 a plain value equal to the
        // filler (0x76A) leaves no slot unique: the copy must still follow the edit.
        Load(editor,original);for(int i=0;i<4;++i)editor.DuplicateStructure(EditorT8Retail::Type(12),1,1);
        auto document=editor.BuildSaveSnapshot();auto* outer=reinterpret_cast<TKMovesetHeader*>(document.data());
        R::Header header;memcpy(&header,document.data()+outer->moveset_data_start,sizeof(header));
        R::Entry moves;memcpy(&moves,document.data()+outer->moveset_data_start+header.directoryOffset+12*sizeof(moves),sizeof(moves));
        const size_t row5=outer->moveset_data_start+moves.offset+5*moves.stride;
        uint64_t key;memcpy(&key,document.data()+row5+0x58+8,8);Put(document,row5+0x58,EditorT8Retail::Encode(0x76A,key));
        for(size_t block:{0x20U,0x58U})for(size_t slot=0;slot<4;++slot)Put<uint32_t>(document,row5+block+0x10+slot*4,0x76A);
        outer->crc32=Helpers::CalculateCrc32(document.data()+outer->moveset_data_start,document.size()-outer->moveset_data_start);
        Load(editor,document);
        using Slots=std::array<uint32_t,4>;
        auto slots=[&](uint32_t row,uint32_t block){Slots s;memcpy(s.data(),editor.Record(12,row).data()+block+0x10,sizeof(s));return s;};
        VectorSet<std::string> order;auto fields=editor.RecordFields(12,5,order);Set(fields,"vulnerability","2114");editor.SaveRecord(12,5,fields);
        Check(slots(5,0x58)==Slots{0x76A,0x842,0x76A,0x76A},"Vulnerability plain copy was not written to slot row % 8 - 4");
        const auto untouched=slots(2,0x58);order.clear();fields=editor.RecordFields(12,2,order);Set(fields,"vulnerability","2114");editor.SaveRecord(12,2,fields);
        Check(slots(2,0x58)==untouched,"A row without a plain copy had its slots changed");
        editor.SetAnimation(5,0);
        uint64_t value;memcpy(&value,editor.Record(12,5).data()+0x20,8);memcpy(&key,editor.Record(12,5).data()+0x28,8);
        Check(EditorT8Retail::Decode(value,key)==0x20U && slots(5,0x20)==Slots{0x76A,0x20,0x76A,0x76A},"Animation change copied the donor's slots instead of this row's");
        const auto names=slots(5,0);editor.RenameMove(5,"CUSTOM_Slot");const auto hash=editor.MoveNameHash(5);
        Check(hash && slots(5,0)==Slots{names[0],*hash,names[2],names[3]},"Rename left the name's plain copy stale");
    }
    Load(editor,Fixture(false));Check(editor.Count(12)==2,"Tables-only retail snapshot failed");
}
// Headless regression input for the PANM commands: doc.tkmvst (moves 0/1 on body instances 0x100/0x101),
// native.panm (importable, not embedded), encoded.panm (re-encoded native.panm) and partial.panm (12 bones, rewritten
// over a native template on import).
int WriteRetailPanmFixture(const std::wstring& output) {
    const std::filesystem::path directory(output);std::filesystem::create_directories(directory);
    const auto native=NativeLikePanm(7,3);
    Check(FileIO::WriteAtomically(directory/"doc.tkmvst",PanmFixture(NativeLikePanm(5,1),NativeLikePanm(9,2))) &&
        FileIO::WriteAtomically(directory/"native.panm",native) &&
        FileIO::WriteAtomically(directory/"encoded.panm",RetailPanm::Encode(RetailPanm::DecodeRaw(native))) &&
        FileIO::WriteAtomically(directory/"partial.panm",NativeLikePanm(7,3,12)),"Cannot write PANM fixture");
    std::cout<<nlohmann::json({{"ok",true}}).dump()<<'\n';return 0;
}
int RenderRetailEditorFile(const std::wstring& input,const std::wstring& output,bool references,bool lists) {
    Localization::LoadFile("en-US");
    movesetInfo info{};info.filename=input;info.name=std::filesystem::path(input).stem().string();
    ImGuiContext* context=ImGui::CreateContext();AppTheme::Apply();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1440,920);io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    auto atlas=std::filesystem::path(output);atlas+=L".rgba";
    FileIO::WriteAtomically(atlas,std::span<const Byte>(pixels,static_cast<size_t>(width)*height*4));
    nlohmann::json result{{"width",1440},{"height",920},{"atlas_width",width},{"atlas_height",height},{"lists",nlohmann::json::array()}};
    {EditorVisualsT8Retail view(&info,nullptr,nullptr);
        for(int frame=0;frame<5;++frame){ImGui::NewFrame();ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);view.Render(700);if(frame==0){view.OpenRecord(0,0);view.OpenRecord(6,0);view.OpenRecord(1,0);view.OpenRecord(12,11);}if(frame==1 && references)view.FindReferences(12,11);if(frame==1 && lists)view.OpenRecord(6,127);ImGui::Render();}
        const auto* draw=ImGui::GetDrawData();
        for(int i=0;i<draw->CmdListsCount;++i){const auto* list=draw->CmdLists[i];nlohmann::json item{{"vertices",nlohmann::json::array()},{"indices",nlohmann::json::array()},{"commands",nlohmann::json::array()}};
            for(const auto& v:list->VtxBuffer)item["vertices"].push_back({v.pos.x,v.pos.y,v.uv.x,v.uv.y,v.col});
            for(auto index:list->IdxBuffer)item["indices"].push_back(index);
            for(const auto& cmd:list->CmdBuffer)if(!cmd.UserCallback)item["commands"].push_back({cmd.ElemCount,cmd.IdxOffset,cmd.VtxOffset,cmd.ClipRect.x,cmd.ClipRect.y,cmd.ClipRect.z,cmd.ClipRect.w});
            result["lists"].push_back(std::move(item));
        }
    }ImGui::DestroyContext(context);Localization::Clear();
    Check(FileIO::WriteAtomically(output,result.dump()),"Cannot write editor draw data");return 0;
}
int VerifyRetailEditorFile(const std::wstring& input,const std::wstring& output) {
    namespace fs=std::filesystem;
    if(fs::absolute(input).lexically_normal()==fs::absolute(output).lexically_normal() || (fs::exists(output)&&fs::equivalent(input,output))) throw std::invalid_argument("Verification output must differ from input");
    GameProcess process;GameData data;auto* game=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);EditorT8Retail editor(process,data,game);
    { MovesetFile source(input);source.Validate();auto* copy=static_cast<Byte*>(malloc(source.Bytes().size()));if(!copy)throw std::bad_alloc();memcpy(copy,source.Bytes().data(),source.Bytes().size());Check(editor.LoadMoveset(copy,source.Bytes().size()),"Real retail file rejected by editor"); }
    {
        auto crc=editor.CalculateCRC32();EditorLogic::Mutation transaction(editor,true);
        for(size_t table=0;table<20;++table) if(editor.Count(table)) { VectorSet<std::string> order;auto fields=editor.RecordFields(table,0,order);editor.SaveRecord(table,0,fields); }
        Check(editor.CalculateCRC32()==crc,"No-op real retail fields altered payload");transaction.Commit();
    }
    uint64_t size;auto* whole=editor.GetMoveset(size);auto* outer=reinterpret_cast<const TKMovesetHeader*>(whole);
    const size_t prefix=outer->moveset_data_start;const size_t offset=editor.Record(1,0).data()-whole+4;uint32_t old=0;memcpy(&old,whole+offset,4);
    const auto left=Helpers::CalculateCrc32(whole+prefix,offset-prefix),right=Helpers::CalculateCrc32(whole+offset+4,size-offset-4);
    VectorSet<std::string> order;auto fields=editor.RecordFields(1,0,order);auto text=std::to_string(old+1);strcpy_s(fields.at("parameter_1")->buffer,FORM_BUFSIZE,text.c_str());editor.SaveRecord(1,0,fields);
    whole=editor.GetMoveset(size);Check(Helpers::CalculateCrc32(whole+prefix,offset-prefix)==left && Helpers::CalculateCrc32(whole+offset+4,size-offset-4)==right,"Real retail edit changed unrelated payload/assets");
    FileJobRequest request;request.output=output;request.bytes=editor.BuildSaveSnapshot();request.codec=TKMovesetCompressionType_LZ4;auto result=RunFileJob(std::move(request));Check(result.ok,result.error.c_str());
    MovesetFile saved(output);saved.Validate();auto payload=saved.Payload();auto changed=offset-prefix;uint32_t value=0;memcpy(&value,payload.data()+changed,4);
    Check(value==old+1 && Helpers::CalculateCrc32(payload.data(),changed)==left && Helpers::CalculateCrc32(payload.data()+changed+4,payload.size()-changed-4)==right,"Real retail save/reopen changed unrelated payload/assets");
    Check(Helpers::CalculateCrc32(payload.data(),payload.size())==saved.Header().crc32,"Reopened retail CRC mismatch");
    size_t decoded=0;for(uint32_t i=0;i<editor.Count(12);++i)if(editor.MoveNameHash(i))++decoded;
    std::cout<<nlohmann::json({{"ok",true},{"moves",editor.Count(12)},{"decoded_move_names",decoded},{"unchanged_other_payload",true},{"live_editing",false}}).dump()<<'\n';return 0;
}

int VerifyRetailEditorStructure(const std::wstring& input,const std::wstring& output){
    namespace fs=std::filesystem;if(fs::absolute(input).lexically_normal()==fs::absolute(output).lexically_normal() || (fs::exists(output)&&fs::equivalent(input,output)))throw std::invalid_argument("Verification output must differ from input");
    MovesetFile file(input);file.Validate();GameProcess process;GameData game;auto* profile=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);EditorT8Retail editor(process,game,profile);Load(editor,std::vector<Byte>(file.Bytes().begin(),file.Bytes().end()));
    const auto originalMoves=editor.Count(12);auto idle=editor.ResolveMove(0x8001);uint32_t clone=0;
    editor.MutateBatch([&]{
        clone=editor.DuplicateStructure(EditorT8Retail::Type(12),idle,1);editor.RenameMove(clone,"tkm_parity_idle");editor.SetAnimation(clone,idle);
        VectorSet<std::string> order;auto fields=editor.RecordFields(12,clone,order);const auto cancels=std::stoul(fields.at("cancels")->buffer);
        const auto copy=editor.DuplicateStructure(EditorT8Retail::Type(6),cancels,editor.ListSize(6,cancels));Set(fields,"cancels",std::to_string(copy).c_str());editor.SaveRecord(12,clone,fields);
        editor.CreateNew(EditorT8Retail::Type(1));editor.CreateNew(EditorT8Retail::Type(9));
    });
    auto bytes=editor.BuildSaveSnapshot();FileJobRequest request;request.output=output;request.bytes=bytes;request.codec=TKMovesetCompressionType_LZ4;auto result=RunFileJob(std::move(request));Check(result.ok,result.error.c_str());
    MovesetFile saved(output);saved.Validate();EditorT8Retail reopened(process,game,profile);Load(reopened,std::vector<Byte>(saved.Bytes().begin(),saved.Bytes().end()));
    Check(reopened.Count(12)==originalMoves+1 && reopened.CustomName(*reopened.MoveNameHash(clone)) && *reopened.CustomName(*reopened.MoveNameHash(clone))=="tkm_parity_idle","Real structural save/reload failed");
    auto before=R::Assets(file.Payload()),after=R::Assets(saved.Payload());Check(before.size()==after.size(),"Structural edit changed animation directory count");size_t checked=0;
    for(size_t i=0;i<before.size();++i){Check(before[i].type==after[i].type && before[i].instance==after[i].instance && before[i].size==after[i].size && !memcmp(file.Payload().data()+before[i].offset,saved.Payload().data()+after[i].offset,before[i].size),"Structural edit changed an animation resource");checked+=before[i].size;}
    std::cout<<nlohmann::json({{"ok",true},{"move",clone},{"idle",idle},{"moves",reopened.Count(12)},{"animation_bytes_preserved",checked},{"game_writes",0}}).dump()<<'\n';return 0;
}

// ---------------------------------------------------------------------------------------------
// Retail editor UI tests driven by Dear ImGui Test Engine (vcpkg imgui[test-engine]).
// Same in-memory context as RenderRetailEditorFile; the engine simulates mouse and keyboard
// through ImGuiIO, so no window, backend, desktop input or game process is involved.
// ---------------------------------------------------------------------------------------------
#include <imgui_te_engine.h>
#include <imgui_te_context.h>
#include <imgui_te_coroutine.h>
#include <format>
#include <string_view>

namespace {
struct RetailUiHarness { EditorVisualsT8Retail* view=nullptr; } g_retailUi;
// Items are addressed by the label the engine recorded (ImGuiTestItemInfo::DebugLabel), gathered from a
// window and its children, because the editor builds most IDs from pointers (fields, list rows).
struct Found { ImGuiID id=0; int index=-1; };
Found FindLabel(ImGuiTestContext* ctx,ImGuiID parent,const std::function<bool(std::string_view)>& match,int from=0) {
    ImGuiTestItemList items;ctx->GatherItems(&items,parent);
    int index=0;
    for(const auto& info:items){if(index>=from && match(std::string_view(info.DebugLabel)))return {info.ID,index};++index;}
    ctx->LogInfo("No item matched among %d gathered items",index);
    for(const auto& info:items){std::string hex;for(size_t i=0;i<strnlen(info.DebugLabel,sizeof(info.DebugLabel)) && i<12;++i)hex+=std::format("{:02X} ",uint8_t(info.DebugLabel[i]));ctx->LogInfo("  '%s' len %zu [%s]",info.DebugLabel,strnlen(info.DebugLabel,sizeof(info.DebugLabel)),hex.c_str());}
    return {};
}
bool AnyTooltipShown() {
    for(auto* window:ImGui::GetCurrentContext()->Windows)
        if((window->Flags&ImGuiWindowFlags_Tooltip) && (window->Active || window->WasActive) && window->Size.y>ImGui::GetTextLineHeight()*2)return true;
    return false;
}
ImGuiWindow* RecordWindow(size_t table,uint32_t row) { return ImGui::FindWindowByName(g_retailUi.view->RecordWindowId(table,row).c_str()); }
// Vertices drawn last frame in the changed-field tint, in a window and its child windows.
int ChangedTintVertices(ImGuiWindow* window) {
    int count=0;
    for(auto* other:ImGui::GetCurrentContext()->Windows)
        if(other==window || other->RootWindow==window)for(const auto& vertex:other->DrawList->VtxBuffer)count+=vertex.col==FORM_INPUT_CHANGED;
    return count;
}
// With TKM_UI_FRAME_DUMP=PATH.json, writes the last frame's draw lists (root windows in display order, then their
// children) and PATH.json.rgba (font atlas), the format of retail-editor-render, for offscreen rasterizing.
void DumpLastFrame() {
    char* path=nullptr;size_t length=0;
    if(_dupenv_s(&path,&length,"TKM_UI_FRAME_DUMP") || !path)return;
    const std::string output(path);free(path);
    auto& io=ImGui::GetIO();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    FileIO::WriteAtomically(output+".rgba",std::span<const Byte>(pixels,size_t(width)*height*4));
    nlohmann::json result{{"width",int(io.DisplaySize.x)},{"height",int(io.DisplaySize.y)},{"atlas_width",width},{"atlas_height",height},{"lists",nlohmann::json::array()}};
    std::function<void(ImGuiWindow*)> add=[&](ImGuiWindow* window){
        if(!window->WasActive || window->Hidden)return;
        nlohmann::json item{{"vertices",nlohmann::json::array()},{"indices",nlohmann::json::array()},{"commands",nlohmann::json::array()}};
        for(const auto& v:window->DrawList->VtxBuffer)item["vertices"].push_back({v.pos.x,v.pos.y,v.uv.x,v.uv.y,v.col});
        for(auto index:window->DrawList->IdxBuffer)item["indices"].push_back(index);
        for(const auto& cmd:window->DrawList->CmdBuffer)if(!cmd.UserCallback)item["commands"].push_back({cmd.ElemCount,cmd.IdxOffset,cmd.VtxOffset,cmd.ClipRect.x,cmd.ClipRect.y,cmd.ClipRect.z,cmd.ClipRect.w});
        result["lists"].push_back(std::move(item));
        for(auto* child:window->DC.ChildWindows)add(child);
    };
    for(auto* window:ImGui::GetCurrentContext()->Windows)if(!(window->Flags&ImGuiWindowFlags_ChildWindow))add(window);
    FileIO::WriteAtomically(output,result.dump());
}
ImGuiWindow* OpenPopupWindow() {
    auto& g=*ImGui::GetCurrentContext();
    for(int i=g.OpenPopupStack.Size-1;i>=0;--i)if(g.OpenPopupStack[i].Window)return g.OpenPopupStack[i].Window;
    return nullptr;
}
void RegisterRetailUiTests(ImGuiTestEngine* engine) {
    ImGuiTest* test=IM_REGISTER_TEST(engine,"retail_editor","animation_library_tab");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(12,0);ctx->Yield(2);
        auto* window=RecordWindow(12,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto button=FindLabel(ctx,window->ID,[](std::string_view label){return label=="Animation";});IM_CHECK(button.id!=0);
        ctx->ItemClick(button.id);ctx->Yield(2);
        auto* popup=OpenPopupWindow();IM_CHECK(popup!=nullptr);
        ctx->SetRef(popup);
        // Tab items are addressed by path (tab bar id / label); the engine records no label for them.
        ctx->ItemClick("##animationTabs/Library (editor_library)");ctx->Yield(2);
        Found folder;
        for(int tries=0;tries<400 && folder.id==0;++tries){ctx->Yield(5);folder=FindLabel(ctx,popup->ID,[](std::string_view label){return label.starts_with("_ui_test - 1");});}
        IM_CHECK(folder.id!=0);
        ctx->ItemOpen(folder.id);ctx->Yield(2);
        auto import=FindLabel(ctx,popup->ID,[](std::string_view label){return label=="Import in move";});IM_CHECK(import.id!=0);
        ctx->ItemClick(import.id);ctx->Yield(2);
        auto cancel=FindLabel(ctx,popup->ID,[](std::string_view label){return label=="Cancel custom PANM";});IM_CHECK(cancel.id!=0);
        // Drop the pending import so the form is clean for the next test, and check the control goes away.
        ctx->ItemClick(cancel.id);ctx->Yield(2);
        cancel=FindLabel(ctx,popup->ID,[](std::string_view label){return label=="Cancel custom PANM";});IM_CHECK(cancel.id==0);
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","cancel_target_validation");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(6,0);ctx->Yield(2);
        auto* window=RecordWindow(6,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto row=[](std::string_view label){return label.starts_with("0  ");};
        auto node=FindLabel(ctx,window->ID,row);IM_CHECK(node.id!=0);
        ctx->ItemOpen(node.id);ctx->Yield(2);
        auto label=FindLabel(ctx,window->ID,[](std::string_view text){return text=="Move ID";});IM_CHECK(label.id!=0);
        auto input=FindLabel(ctx,window->ID,[](std::string_view text){return text=="##";},label.index+1);IM_CHECK(input.id!=0);
        ctx->ItemInputValue(input.id,"60000");ctx->Yield(2);
        node=FindLabel(ctx,window->ID,row);IM_CHECK(node.id!=0);
        auto info=ctx->ItemInfo(node.id);
        IM_CHECK(std::string_view(info.DebugLabel).find("INVALID")!=std::string_view::npos);
        ctx->ItemInputValue(input.id,"1");ctx->Yield(2);
        info=ctx->ItemInfo(node.id);
        IM_CHECK(std::string_view(info.DebugLabel).find("INVALID")==std::string_view::npos);
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","hit_level_combo");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(12,0);ctx->Yield(2);
        auto* window=RecordWindow(12,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        // The move form shows one decoder combo while the hitbox groups stay collapsed: the hit level.
        auto combo=FindLabel(ctx,window->ID,[](std::string_view label){return label=="codec:hit_level";});IM_CHECK(combo.id!=0);
        ctx->ItemClick(combo.id);ctx->Yield(2);
        auto* popup=OpenPopupWindow();IM_CHECK(popup!=nullptr);
        auto high=FindLabel(ctx,popup->ID,[](std::string_view label){return label.starts_with("High (0x512)");});IM_CHECK(high.id!=0);
        ctx->ItemClick(high.id);ctx->Yield(2);
        const auto* form=g_retailUi.view->RecordForm(12,0);IM_CHECK(form!=nullptr);
        ctx->LogInfo("after combo: unsaved=%d error='%s'",int(form->unsavedChanges),form->OperationError().c_str());
        auto apply=FindLabel(ctx,window->ID,[](std::string_view label){return label=="Apply";});IM_CHECK(apply.id!=0);
        ctx->ItemClick(apply.id);ctx->Yield(3);
        ctx->LogInfo("after apply: unsaved=%d error='%s'",int(form->unsavedChanges),form->OperationError().c_str());
        auto record=g_retailUi.view->Editor()->Record(12,0);uint64_t value,key;memcpy(&value,record.data()+0x78,8);memcpy(&key,record.data()+0x80,8);
        auto decoded=EditorT8Retail::Decode(value,key);IM_CHECK(decoded.has_value());
        ctx->LogInfo("hit_level after Apply: 0x%X",*decoded);
        IM_CHECK((*decoded&0xFFF)==0x512 && (*decoded>>12)==(0x78u>>12));
        // Apply replaces the form's field objects with clones: the decoder combos are keyed by field name and stay.
        ctx->Yield(2);
        IM_CHECK(FindLabel(ctx,window->ID,[](std::string_view label){return label=="codec:hit_level";}).id!=0);
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","parameter_labels");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(1,0);ctx->Yield(2);
        auto* window=RecordWindow(1,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto node=FindLabel(ctx,window->ID,[](std::string_view label){return label.starts_with("0  ");});IM_CHECK(node.id!=0);
        ctx->ItemOpen(node.id);ctx->Yield(2);
        // Row 0 draws condition, parameter_1..4 in order; the condition input is the first "##" after the node.
        auto condition=FindLabel(ctx,window->ID,[](std::string_view text){return text=="##";},node.index+1);IM_CHECK(condition.id!=0);
        const auto* form=g_retailUi.view->RecordForm(1,0);IM_CHECK(form!=nullptr);
        ctx->ItemInputValue(condition.id,"220");ctx->Yield(2);
        ctx->LogInfo("parameter_1 label: '%s'",form->FieldDisplayName(0,"parameter_1").c_str());
        IM_CHECK(form->FieldDisplayName(0,"parameter_1").starts_with("Parameter 1: u16 fighter id"));
        ctx->ItemInputValue(condition.id,"68");ctx->Yield(2);
        IM_CHECK(form->FieldDisplayName(0,"parameter_1")=="Parameter 1 (unused)");
        ctx->ItemInputValue(condition.id,"0");ctx->Yield(2);
        IM_CHECK(form->FieldDisplayName(0,"parameter_1")=="Parameter 1");
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","requirement_id_picker");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(1,0);ctx->Yield(2);
        auto* window=RecordWindow(1,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto node=FindLabel(ctx,window->ID,[](std::string_view label){return label.starts_with("0  ");});IM_CHECK(node.id!=0);
        ctx->ItemOpen(node.id);ctx->Yield(2);
        auto picker=FindLabel(ctx,window->ID,[](std::string_view label){return label=="codec:condition";});IM_CHECK(picker.id!=0);
        ctx->ItemClick(picker.id);ctx->Yield(2);
        auto* popup=OpenPopupWindow();IM_CHECK(popup!=nullptr);
        auto filter=FindLabel(ctx,popup->ID,[](std::string_view label){return label=="##filter";});IM_CHECK(filter.id!=0);
        ctx->ItemInputValue(filter.id,"CharacterIdEq");ctx->Yield(2);
        auto entry=FindLabel(ctx,popup->ID,[](std::string_view label){return label.starts_with("220  ");});IM_CHECK(entry.id!=0);
        ctx->ItemClick(entry.id);ctx->Yield(3);
        const auto* form=g_retailUi.view->RecordForm(1,0);IM_CHECK(form!=nullptr);
        ctx->LogInfo("parameter_1 after picker: '%s'",form->FieldDisplayName(0,"parameter_1").c_str());
        IM_CHECK(form->FieldDisplayName(0,"parameter_1").starts_with("Parameter 1: u16 fighter id"));
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","changed_field_tint");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        // Move 1 is not opened by the other tests. A field is tinted while its text differs from the last applied value.
        g_retailUi.view->OpenRecord(12,1);ctx->Yield(2);
        auto* window=RecordWindow(12,1);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto settle=[&]{ctx->MouseMoveToPos(ImVec2(5,5));ctx->Yield(3);return ChangedTintVertices(window);};
        IM_CHECK_EQ(settle(),0);
        // Transition is a reference field (normally drawn in the reference colour) in the always-open category.
        auto label=FindLabel(ctx,window->ID,[](std::string_view text){return text.starts_with("Transition");});IM_CHECK(label.id!=0);
        auto input=FindLabel(ctx,window->ID,[](std::string_view text){return text=="##";},label.index+1);IM_CHECK(input.id!=0);
        ctx->ItemInputValue(input.id,"1");
        IM_CHECK_GT(settle(),0);
        DumpLastFrame();
        auto apply=FindLabel(ctx,window->ID,[](std::string_view text){return text=="Apply";});IM_CHECK(apply.id!=0);
        ctx->ItemClick(apply.id);
        IM_CHECK_EQ(settle(),0);
        // Apply commits cloned fields (EditorForm transactions), so the input has a new ID.
        label=FindLabel(ctx,window->ID,[](std::string_view text){return text.starts_with("Transition");});IM_CHECK(label.id!=0);
        input=FindLabel(ctx,window->ID,[](std::string_view text){return text=="##";},label.index+1);IM_CHECK(input.id!=0);
        ctx->ItemInputValue(input.id,"0");
        IM_CHECK_GT(settle(),0);
        ctx->ItemInputValue(input.id,"1");
        IM_CHECK_EQ(settle(),0);
    };
    test=IM_REGISTER_TEST(engine,"retail_editor","field_description_tooltip");
    test->TestFunc=[](ImGuiTestContext* ctx) {
        g_retailUi.view->OpenRecord(6,0);ctx->Yield(2);
        auto* window=RecordWindow(6,0);IM_CHECK(window!=nullptr);
        ctx->SetRef(window);
        auto node=FindLabel(ctx,window->ID,[](std::string_view label){return label.starts_with("0  ");});IM_CHECK(node.id!=0);
        ctx->ItemOpen(node.id);ctx->Yield(2);
        auto label=FindLabel(ctx,window->ID,[](std::string_view text){return text=="Move ID";});IM_CHECK(label.id!=0);
        ctx->MouseMove(label.id);
        ctx->SleepNoSkip(0.8f,1.0f/60.0f);
        IM_CHECK(AnyTooltipShown());
        ctx->MouseMoveToPos(ImVec2(5,5));ctx->SleepNoSkip(0.3f,1.0f/60.0f);
        IM_CHECK(!AnyTooltipShown());
    };
}
}
int RunRetailEditorUiTests() {
    Localization::LoadFile("en-US");
    // Static decoder checks (RetailFieldCodecs.hpp).
    Check(RetailFieldCodecs::CancelOptionText(0x50)=="has command | bit 0x40?","cancel option 0x50");
    Check(RetailFieldCodecs::CancelOptionText(0x101)=="no command | alias target","cancel option 0x101");
    Check(RetailFieldCodecs::CancelOptionText(0)=="0 (not filled: set by the game at load)","cancel option 0");
    Check(RetailFieldCodecs::HitLevelText(0x200512)=="High (0x512) + flags 0x200","hit level text");
    Check(RetailFieldCodecs::HitboxLocationText(0x809)=="Right forearm [R_LowerArm] -> Right hand [R_Hand]","hitbox location text");
    Check(RetailFieldCodecs::HitboxLocationText(0x16)=="Head top [ATH_Head_Top]" && RetailFieldCodecs::HitboxLocationText(0x956)=="Right hand [R_Hand] -> item1","hitbox location limb table");
    Check(RetailFieldCodecs::HitboxLocationText(0x40D,false)=="Neck>L_Hand","hitbox location compact");
    Check(RetailFieldCodecs::LimbName(0x7D)=="limb 0x7D [0x7392ED95]" && RetailFieldCodecs::LimbName(0x86)=="limb 0x86 [none]","hitbox location unresolved limbs");
    {
        auto name=[](uint32_t id){return "C"+std::to_string(id);};
        using RetailFieldCodecs::RequirementParamText;
        Check(RequirementParamText(220,6,name)=="C6" && RequirementParamText(223,0x88,name)=="alias: C5" &&
            RequirementParamText(220,0x84,name)=="alias, never matches" && RequirementParamText(224,6,name).empty(),"requirement fighter ids");
        Check(RequirementParamText(326,0x2E0001,name)=="flag 46 <= 1" && RequirementParamText(453,0x43,name)=="1+2+RA" &&
            RequirementParamText(672,1,name)=="Practice" && RequirementParamText(229,1,name)=="CPU" &&
            RequirementParamText(668,0x23,name)=="chapter 2, battle 3" && RequirementParamText(100,1,name).empty(),"requirement parameter codes");
    }
    Check(RetailFieldCodecs::VulnerabilityText(0x842)=="Standing + 0x840","vulnerability standing");
    Check(RetailFieldCodecs::VulnerabilityText(0xC842)=="Standing + 0xC840","vulnerability jump");
    Check(RetailFieldCodecs::VulnerabilityText(0x7402)=="Standing + airborne + 0x7000","vulnerability knocked down");
    Check(RetailFieldCodecs::VulnerabilityText(0x01006084)=="Downed + 0x6080 + flags 0x100","vulnerability downed");
    const auto testDirectory=std::filesystem::temp_directory_path()/std::format("tkmovesets-ui-test-{}-{}",GetCurrentProcessId(),Helpers::getCurrentTimestamp());
    Check(std::filesystem::create_directory(testDirectory),"Cannot create the UI test directory");
    const auto temporary=testDirectory/"document.tkmvst";
    Check(FileIO::WriteAtomically(temporary,Fixture()),"Cannot write the UI test document");
    const auto library=std::filesystem::absolute(EDITOR_LIB_DIRECTORY)/"_ui_test";
    std::filesystem::create_directories(library);
    Check(FileIO::WriteAtomically(library/"anim.panm",NativeLikePanm(7,3)),"Cannot write the UI test animation");
    movesetInfo info{};info.filename=temporary.wstring();info.name="ui-test";
    int tested=0,success=0;
    ImGuiContext* context=ImGui::CreateContext();AppTheme::Apply();
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1440,920);io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.DeltaTime=1.0f/60.0f;
    {unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);}
    ImGuiTestEngine* engine=ImGuiTestEngine_CreateContext();
    auto& engineIo=ImGuiTestEngine_GetIO(engine);
    engineIo.ConfigVerboseLevel=ImGuiTestVerboseLevel_Info;engineIo.ConfigVerboseLevelOnError=ImGuiTestVerboseLevel_Debug;
    engineIo.ConfigRunSpeed=ImGuiTestRunSpeed_Fast;engineIo.ConfigNoThrottle=true;engineIo.ConfigLogToTTY=true;
    engineIo.CoroutineFuncs=Coroutine_ImplStdThread_GetInterface();
    ImGuiTestEngine_Start(engine,context);
    {
        EditorVisualsT8Retail view(&info,nullptr,nullptr);g_retailUi.view=&view;
        RegisterRetailUiTests(engine);
        ImGuiTestEngine_QueueTests(engine,ImGuiTestGroup_Tests,nullptr,ImGuiTestRunFlags_RunFromCommandLine);
        for(int frame=0;frame<60000 && !ImGuiTestEngine_IsTestQueueEmpty(engine);++frame) {
            io.DeltaTime=1.0f/60.0f;ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);
            view.Render(700);
            ImGui::Render();ImGuiTestEngine_PostSwap(engine);
        }
        ImGuiTestEngine_GetResult(engine,tested,success);
        ImGuiTestEngine_Stop(engine);g_retailUi.view=nullptr;
    }
    ImGui::DestroyContext(context);ImGuiTestEngine_DestroyContext(engine);Localization::Clear();
    std::error_code error;std::filesystem::remove_all(library,error);std::filesystem::remove_all(testDirectory,error);
    constexpr int expected=7; // library tab, cancel validation, hit level combo, parameter labels, id picker, changed tint, tooltip
    std::cout<<nlohmann::json({{"ok",tested==expected && success==tested},{"tested",tested},{"success",success}}).dump()<<'\n';
    return tested==expected && success==tested ? 0 : 1;
}
