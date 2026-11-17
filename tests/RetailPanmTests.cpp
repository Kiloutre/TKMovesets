#include "RetailPanm.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Importer_t8/RetailResourceLoader.hpp"
#include "T7AnimToRetailT8.hpp"
#include "RetailT8AnimToT7.hpp"
#include "Animations.hpp"
#include <cmath>
#include "AnimationReader.hpp"
#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void Check(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
// The game objects our animation entries point at, read from the shipped game_addresses.ini (t8-retail).
RetailT8Resources::EngineAddresses ShippedEngine(uint64_t module) {
    GameAddressesFile file; file.Reload();
    GameAddressesWrapper game; game.addrFile=&file; game.gameKey="t8-retail"; game.minorGameKey="t8-retail";
    return RetailT8Resources::ReadEngineAddresses(game,module);
}
template<class F> void Reject(F&& callback) { bool failed=false;try{callback();}catch(const std::exception&){failed=true;}Check(failed,"Invalid animation accepted"); }
template<class T> void Put(std::vector<Byte>& bytes,size_t offset,T value) { memcpy(bytes.data()+offset,&value,sizeof(value)); }

// Reads a locally built compiled animation (RetailT8Resources::BuildCompiledAnimation) through its remote pointers.
struct Remote {
    const std::vector<Byte>& image; uint64_t base;
    template<class T> T Get(uint64_t address) const {
        Check(address>=base && address-base<=image.size() && sizeof(T)<=image.size()-(address-base),"Compiled animation pointer outside its image");
        T value;memcpy(&value,image.data()+(address-base),sizeof(value));return value;
    }
    std::string String(uint64_t at) const {
        if(Get<int8_t>(at+23)<0)return {reinterpret_cast<const char*>(image.data()+(Get<uint64_t>(at)-base)),Get<uint64_t>(at+8)};
        const auto size=23-Get<uint8_t>(at+23);Check(size<=23,"Invalid in-place string");
        return {reinterpret_cast<const char*>(image.data()+(at-base)),size_t(size)};
    }
    // Validates an EASTL rbtree (order, parents, anchor, colors, black height) and returns its nodes in order.
    std::vector<uint64_t> Tree(uint64_t anchor,const std::function<bool(uint64_t,uint64_t)>& less) const {
        std::vector<uint64_t> nodes;
        const auto root=Get<uint64_t>(anchor+0x10);
        Check(Get<uint8_t>(anchor+0x18)==0,"Anchor must be red");
        if(!root) { Check(Get<uint64_t>(anchor)==anchor && Get<uint64_t>(anchor+8)==anchor && !Get<uint64_t>(anchor+0x20),"Invalid empty tree"); return nodes; }
        Check(Get<uint64_t>(root+0x10)==anchor && Get<uint8_t>(root+0x18)==1,"Root must be black with the anchor as parent");
        std::function<int(uint64_t)> walk=[&](uint64_t node)->int {
            if(!node)return 1;
            const auto left=Get<uint64_t>(node+8),right=Get<uint64_t>(node);
            const bool red=Get<uint8_t>(node+0x18)==0;
            for(auto child:{left,right})if(child) {
                Check(Get<uint64_t>(child+0x10)==node,"Broken parent link");
                Check(!red || Get<uint8_t>(child+0x18)==1,"Red node with a red child");
            }
            const auto height=walk(left);nodes.push_back(node);
            Check(walk(right)==height,"Unequal black heights");
            return height+(red?0:1);
        };
        walk(root);
        for(size_t i=1;i<nodes.size();++i)Check(less(nodes[i-1],nodes[i]),"Tree keys out of order");
        Check(Get<uint64_t>(anchor)==nodes.back() && Get<uint64_t>(anchor+8)==nodes.front() && Get<uint64_t>(anchor+0x20)==nodes.size(),
            "Anchor extremes or size wrong");
        return nodes;
    }
};

// Hand-built PANM whose group vector holds `groups` references to one group and whose bone vector holds `bones`
// references to one bone (one track): legal FlatBuffers, every reference forward, each table its own vtable.
std::vector<Byte> AliasedPanm(uint32_t groups,uint32_t bones) {
    std::vector<Byte> b(8);memcpy(b.data()+4,"PANM",4);
    auto u16=[&](size_t at,uint16_t v){memcpy(b.data()+at,&v,2);};
    auto u32=[&](size_t at,uint32_t v){memcpy(b.data()+at,&v,4);};
    auto vtable=[&](std::initializer_list<unsigned> fields){const auto at=b.size();b.resize(at+28);u16(at,26);u16(at+2,48);
        for(auto f:fields)u16(at+4+2*f,static_cast<uint16_t>(4+4*f));return at;};
    auto table=[&](size_t vt){const auto at=(b.size()+3)&~size_t(3);b.resize(at+48);const auto s=static_cast<int32_t>(at-vt);memcpy(b.data()+at,&s,4);return at;};
    auto vector=[&](uint32_t count){const auto at=(b.size()+3)&~size_t(3);b.resize(at+4+4*size_t(count));u32(at,count);return at;};
    auto ref=[&](size_t at,size_t target){u32(at,static_cast<uint32_t>(target-at));};
    const auto vRoot=vtable({2,3}),vAnim=vtable({1,2,3,4,5,6,7,8,9}),vGroup=vtable({1,2}),vBone=vtable({1,2}),vTrack=vtable({1,2,3,4,6,7});
    const auto root=table(vRoot);u32(0,static_cast<uint32_t>(root));b[root+12]=1;
    const auto anim=table(vAnim);ref(root+16,anim);
    const auto groupVector=vector(groups);ref(anim+20,groupVector);
    const auto group=table(vGroup);for(uint32_t i=0;i<groups;++i)ref(groupVector+4+4*i,group);
    const auto boneVector=vector(bones);ref(group+12,boneVector);
    const auto bone=table(vBone);for(uint32_t i=0;i<bones;++i)ref(boneVector+4+4*i,bone);
    const auto trackVector=vector(1);ref(bone+12,trackVector);
    const auto track=table(vTrack);ref(trackVector+4,track);
    const auto name=vector(1);b[name+4]='b';ref(bone+8,name);ref(track+8,name);
    return b;
}
void RunCompiledAnimationTests() {
    namespace P=RetailPanm;namespace R=RetailT8Resources;
    {
        // QA 1.0 (CompiledPanmAliasedVectorsBounded): aliased group/bone vectors would make the compile walk
        // groups x bones x tracks nodes (up to 65536^3); the walk is bounded by MaxCompiledNodes.
        const auto modest=R::BuildCompiledAnimation(AliasedPanm(4,4),0,ShippedEngine(0x140000000ULL));
        Check(!modest.image.empty(),"Aliased PANM fixture does not compile");
        std::string error;
        try { R::BuildCompiledAnimation(AliasedPanm(1024,1024),0,ShippedEngine(0x140000000ULL)); } catch(const std::exception& e) { error=e.what(); }
        Check(error.find("too many bones or tracks")!=std::string::npos,("Aliased PANM vectors were walked unbounded: "+error).c_str());
    }
    constexpr uint64_t module=0x140000000ULL,base=0x7ff000010000ULL;
    const auto engine=ShippedEngine(module);
    // The shipped addresses are those of retail 26155785 (exe patterns in game_addresses.ini find the same values).
    Check(engine.compiledVtable==module+0x75C32F0 && engine.emptyBucket==module+0x959ABD0 && engine.animResourceVtable==module+0x85A59C0 &&
        engine.resourceRefVtable==module+0x858A9C0 && engine.noOp==module+0xA6E230,"game_addresses.ini animation objects changed");
    // 40 bones, including a name too long for the in-place string, built in reverse order to exercise sorting.
    P::Animation animation{4,{}};
    for(int i=39;i>=0;--i) {
        P::Track track{(i==7?std::string("A_bone_name_longer_than_twenty_three_"):std::string("Bone"))+std::to_string(i),
            std::vector<P::Sample>(i%3?4:1),i%5?std::vector<P::TrackOption>{{8,1}}:std::vector<P::TrackOption>{{1,1},{3,1},{4,1},{8,1}}};
        for(size_t f=0;f<track.samples.size();++f)track.samples[f].translation={float(i),float(f),1};
        animation.tracks.push_back(track);
    }
    const auto panm=P::Encode(animation);
    const auto built=R::BuildCompiledAnimation(panm,base,engine);
    Check(R::BuildCompiledAnimation(panm,0x20000,engine).image.size()==built.image.size(),"Compiled layout depends on its address");
    Remote r{built.image,base};const auto o=built.object;
    Check(r.Get<uint64_t>(o)==engine.compiledVtable && r.Get<uint32_t>(o+8)==3 && r.Get<float>(o+0xC)==60.0f &&
        r.Get<uint8_t>(o+0x10)==1 && r.Get<uint8_t>(o+0x18)==0 && r.Get<float>(o+0x1C)==1.0f,"Compiled animation header changed");
    for(uint64_t hash:{o+0x20,o+0x50})
        Check(r.Get<uint64_t>(hash+8)==engine.emptyBucket && r.Get<uint64_t>(hash+0x10)==1 && !r.Get<uint64_t>(hash+0x18),"Hash map is not the empty EASTL map");
    const auto data=r.Get<uint64_t>(o+0xD8);
    Check(data==r.Get<uint64_t>(o+0xB0) && !r.Get<uint64_t>(o+0xE0) && !r.Get<uint64_t>(o+0xE8),"Sample base or owner changed");
    // The PANM is the start of the image; the sample base is its animation field 5.
    Check(!memcmp(built.image.data(),panm.data(),panm.size()) && data>=base && data-base<=panm.size(),"PANM not uploaded in place");
    const auto groups=r.Tree(o+0x80,[&](uint64_t a,uint64_t b){return r.Get<uint8_t>(a+0x20)<r.Get<uint8_t>(b+0x20);});
    Check(groups.size()==1 && r.Get<uint8_t>(groups[0]+0x20)==1,"Bone group changed");
    const auto byName=[&](uint64_t a,uint64_t b){return r.String(a+0x20)<r.String(b+0x20);};
    const auto bones=r.Tree(groups[0]+0x28,byName);
    Check(bones.size()==animation.tracks.size(),"Bone count changed");
    const auto decoded=P::DecodeRaw(panm);
    for(auto bone:bones) {
        const auto name=r.String(bone+0x20);
        const auto source=std::find_if(decoded.tracks.begin(),decoded.tracks.end(),[&](const auto& t){return t.name==name;});
        Check(source!=decoded.tracks.end(),"Compiled bone name not in the PANM");
        const auto tracks=r.Tree(bone+0x38,byName);
        Check(tracks.size()==1 && r.String(tracks[0]+0x20)=="Transform","Track node changed");
        const auto entry=tracks[0]+0x38,count=source->samples.size();
        uint32_t flags=0;for(auto option:source->options)if(option.value)flags|=option.field==8?0x80:1u<<(option.field-1);
        Check(r.Get<uint8_t>(entry)==1 && r.Get<uint8_t>(entry+1)==(count==1?2:1) && r.Get<uint32_t>(entry+4)==count &&
            r.Get<uint32_t>(entry+8)==flags && r.Get<uint32_t>(entry+0x10)==count*sizeof(P::Sample) && !r.Get<uint64_t>(entry+0x18),
            "Track entry changed");
        const auto samples=data+r.Get<uint32_t>(entry+0xC);
        r.Get<Byte>(samples+r.Get<uint32_t>(entry+0x10)-1);
        Check(!memcmp(built.image.data()+(samples-base),source->samples.data(),count*sizeof(P::Sample)),"Track offset does not reach its samples");
    }
    // Small trees: every size from 1 to 33 must give a valid red-black tree.
    for(size_t n=1;n<=33;++n) {
        P::Animation tiny{1,{}};
        for(size_t i=0;i<n;++i)tiny.tracks.push_back({"B"+std::to_string(100+i),std::vector<P::Sample>(1)});
        const auto image=R::BuildCompiledAnimation(P::Encode(tiny),base,engine);
        Remote s{image.image,base};
        const auto group=s.Tree(image.object+0x80,[](uint64_t,uint64_t){return true;});
        Check(s.Tree(group[0]+0x28,[&](uint64_t a,uint64_t b){return s.String(a+0x20)<s.String(b+0x20);}).size()==n,"Small tree lost bones");
    }
    // Inputs the engine or the upload cannot take.
    for(size_t size=0;size+28<=panm.size();size+=97)Reject([&]{R::BuildCompiledAnimation({panm.data(),size},base,engine);});
    Reject([&]{R::BuildCompiledAnimation(panm,base+8,engine);});
    auto changed=panm;changed[4]='X';Reject([&]{R::BuildCompiledAnimation(changed,base,engine);});
}

// PANM -> T7 0xC8 / 0x64 (RetailT8AnimToT7): a synthetic T7 animation in the T8->T7 convention converts to PANM,
// back to T7 in both formats, and to PANM again with every converted bone unchanged (0xC8 float-exact, 0x64
// within its quantization).
RetailPanm::T7Animation SyntheticT7(uint32_t frames) {
    RetailPanm::T7Animation a{frames,23,std::vector<float>(size_t(frames)*23*3),false};
    for(uint32_t f=0;f<frames;++f)for(int ch=0;ch<23;++ch)for(int i=0;i<3;++i) {
        const double t=f*0.07+ch*0.61+i*1.37;
        double v;
        if(RetailT8AnimToT7::ChannelTypes[ch]==11)v=std::sin(t)*(ch==0?150.0:40.0)+f*(ch==0?3.0:0.0);
        else if(ch==5)v=i==1?0:-1.5707963267948966;                  // LowerBody pinned at rest
        else if(ch==3)v=0.08*std::sin(t);                             // FullBody under 10 degrees
        else if(ch==2)v=i==2?f*0.45-3.0:0.05*std::sin(t);             // DirY spins through +-pi repeatedly
        else v=(i==1?0.9:2.6)*std::sin(t)+(f%23==7?0.4:0.0);          // includes single-frame spikes
        a.values[(size_t(f)*23+ch)*3+i]=static_cast<float>(v);
    }
    return a;
}
void ComparePanm(std::span<const Byte> expected,std::span<const Byte> actual,double angle,double distance,const char* what) {
    const auto e=RetailPanm::DecodeRaw(expected),a=RetailPanm::DecodeRaw(actual);
    Check(e.frames==a.frames && e.tracks.size()==a.tracks.size(),what);
    for(size_t t=0;t<e.tracks.size();++t) {
        Check(e.tracks[t].name==a.tracks[t].name && e.tracks[t].samples.size()==a.tracks[t].samples.size(),what);
        for(size_t f=0;f<e.tracks[t].samples.size();++f) {
            const auto& x=e.tracks[t].samples[f];const auto& y=a.tracks[t].samples[f];
            // ~ rotation angle between them (acos of the dot product is too noisy near 1 with float samples).
            double minus=0,plus=0;
            for(int i=0;i<4;++i) { const double d=double(x.rotation[i])-y.rotation[i],s=double(x.rotation[i])+y.rotation[i]; minus+=d*d; plus+=s*s; }
            const double error=2*std::sqrt(std::fmin(minus,plus));
            if(error>angle)throw std::runtime_error(std::string(what)+": rotation of "+e.tracks[t].name+" frame "+std::to_string(f)+" off by "+std::to_string(error));
            for(int i=0;i<3;++i)if(std::fabs(double(x.translation[i])-y.translation[i])>distance)
                throw std::runtime_error(std::string(what)+": translation of "+e.tracks[t].name+" frame "+std::to_string(f));
        }
    }
}
void RunPanmToT7Tests() {
    namespace P=RetailPanm;namespace T=RetailT8AnimToT7;
    for(uint32_t frames:{1u,2u,17u,18u,53u,400u}) {
        const auto panm=T7AnimToRetailT8::Convert(SyntheticT7(frames));
        const auto c8=T::Convert(panm,T::Format::Frames);
        Check(c8[0]==0xC8 && TAnimUtils::FromMemory::Safe::GetAnimSize(c8.data(),c8.size())==c8.size(),"0xC8 output extent");
        const auto fromC8=P::DecodeT7(c8);
        Check(fromC8.frames==frames && fromC8.channels==23 && !fromC8.keyframed,"0xC8 output layout");
        ComparePanm(panm,T7AnimToRetailT8::Convert(fromC8),2e-4,1e-4,"PANM -> 0xC8 -> PANM");
        const auto k64=T::Convert(panm,T::Format::Keyframed);
        Check(k64[0]==0x64 && TAnimUtils::FromMemory::Safe::GetAnimSize(k64.data(),k64.size())==k64.size(),"0x64 output extent");
        const auto from64=P::DecodeT7(k64);
        Check(from64.frames==frames && from64.channels==23 && from64.keyframed,"0x64 output layout");
        ComparePanm(panm,T7AnimToRetailT8::Convert(from64),3e-3,0.01,"PANM -> 0x64 -> PANM");
        if(frames==400)Check(k64.size()<c8.size()/2,"0x64 output is not smaller than 0xC8");
    }
    // Writers refuse what they cannot represent.
    auto keyframed=SyntheticT7(3);keyframed.keyframed=true;Reject([&]{T::WriteC8(keyframed);});
    auto partial=SyntheticT7(3);partial.channels=22;partial.values.resize(3*22*3);Reject([&]{T::Write64(partial);});
    Reject([&]{T::Write64(SyntheticT7(65536));});
    auto nan=SyntheticT7(3);nan.values[5]=std::numeric_limits<float>::quiet_NaN();Reject([&]{T::WriteC8(nan);});
    auto noRoot=P::DecodeRaw(T7AnimToRetailT8::Convert(SyntheticT7(3)));
    std::erase_if(noRoot.tracks,[](const P::Track& t){return t.name=="Top";});Reject([&]{T::ToChannels(noRoot);});
    // Native T8 layout (issue #178): MUKI is T7 DirY, HARA_ROT1 is T7 FullBody (the juggle/down body state).
    {
        constexpr double HalfPi=1.5707963267948966;
        auto body=[&](bool native,bool turning) {
            auto a=SyntheticT7(6);
            for(uint32_t f=0;f<6;++f) {
                auto at=[&](int ch,int i)->float&{return a.values[(size_t(f)*23+ch)*3+i];};
                for(int i=0;i<3;++i)at(2,i)=at(3,i)=0;
                at(2,1)=static_cast<float>(2*HalfPi);                    // DirY: back-turned (180 degree yaw)
                at(3,0)=static_cast<float>(turning && f>=3?HalfPi:0.0);  // FullBody: face up from frame 3
                if(native)at(5,1)=0.2f;                                  // LowerBody off its rest: native T7 layout
            }
            return P::DecodeRaw(T7AnimToRetailT8::Convert(a));
        };
        auto track=[](const P::Animation& a,const char* name)->const P::Track&{
            for(const auto& t:a.tracks)if(t.name==name)return t;
            throw std::runtime_error(std::string("Converted animation has no ")+name);
        };
        auto sameRotation=[](const P::Sample& s,std::array<double,4> q){
            double minus=0,plus=0;
            for(int i=0;i<4;++i){minus+=(s.rotation[i]-q[i])*(s.rotation[i]-q[i]);plus+=(s.rotation[i]+q[i])*(s.rotation[i]+q[i]);}
            return std::fmin(minus,plus)<1e-8;
        };
        const std::array<double,4> identity{0,0,0,1},backTurned{0,1,0,0},faceUp{std::sin(HalfPi/2),0,0,std::cos(HalfPi/2)};
        const auto turning=body(true,true);
        const auto& hara=track(turning,"HARA_ROT1");const auto& muki=track(turning,"MUKI");
        Check(hara.samples.size()==6,"HARA_ROT1 is not animated when FullBody turns");
        for(size_t f=0;f<6;++f) {
            Check(sameRotation(hara.samples[f],f>=3?faceUp:identity),"HARA_ROT1 does not carry FullBody");
            Check(sameRotation(muki.samples[muki.samples.size()==1?0:f],backTurned),"MUKI does not carry DirY alone");
        }
        const auto still=body(true,false);
        Check(track(still,"HARA_ROT1").samples.size()==1 && sameRotation(track(still,"HARA_ROT1").samples[0],identity),"Still HARA_ROT1 is not one upright sample");
        const auto fromT8=body(false,true);
        Check(track(fromT8,"HARA_ROT1").samples.size()==1 && sameRotation(track(fromT8,"HARA_ROT1").samples[0],identity),
            "HARA_ROT1 is not upright for the pelvis-in-FullBody layout");
    }
}
}
void RunRetailPanmTests() {
    namespace P=RetailPanm;
    P::Animation animation{3,{{"Top",std::vector<P::Sample>(3),{{8,1}}},{"Hip",std::vector<P::Sample>(1),{{1,1},{3,1},{4,1},{8,1}}}}};
    animation.tracks[0].samples[0].translation={1,2,3};
    animation.tracks[0].samples[1].translation={4,5,6};
    animation.tracks[0].samples[2].translation={7,8,9};
    auto bytes=P::Encode(animation);
    Check(RetailT8Snapshot::PanmSize(bytes)==bytes.size(),"Generated PANM failed native snapshot extent validation");
    auto decoded=P::DecodeRaw(bytes);
    Check(decoded.frames==3 && decoded.tracks.size()==2 && decoded.tracks[0].name=="Top" && decoded.tracks[0].options==animation.tracks[0].options &&
        decoded.tracks[1].options==animation.tracks[1].options &&
        decoded.tracks[1].name=="Hip" && decoded.tracks[1].samples.size()==1,"PANM schema or static samples changed");
    for(size_t frame=0;frame<3;++frame)Check(!memcmp(&decoded.tracks[0].samples[frame],&animation.tracks[0].samples[frame],44),"PANM transform bits changed");
    Check(P::Encode(decoded)==bytes,"PANM serialization is nondeterministic");
    for(size_t size=0;size<bytes.size();++size)Reject([&]{P::DecodeRaw({bytes.data(),size});});
    auto changed=bytes;Put<uint32_t>(changed,0,UINT32_MAX);Reject([&]{P::DecodeRaw(changed);});
    {
        // QA 1.0 (PanmDecodeSharedSampleBudget): tracks that all point at one sample range must not decode to
        // more than the 32 MiB PANM budget (255 bones x 32 MiB before the fix).
        constexpr uint32_t frames=400000; // 17.6 MB of samples for the one full track
        P::Animation wide{frames,{{"Top",std::vector<P::Sample>(frames)},{"Hip",std::vector<P::Sample>(1)},{"Leg",std::vector<P::Sample>(1)}}};
        auto shared=P::Encode(wide);
        Check(P::DecodeRaw(shared).tracks.size()==3,"Wide PANM fixture does not decode");
        auto u32=[&](size_t at){uint32_t v;memcpy(&v,shared.data()+at,4);return v;};
        auto field=[&](size_t table,unsigned index){const auto vt=table-static_cast<int32_t>(u32(table));uint16_t off;memcpy(&off,shared.data()+vt+4+2*index,2);Check(off!=0,"Missing PANM test field");return table+off;};
        auto object=[&](size_t table,unsigned index){const auto at=field(table,index);return at+u32(at);};
        auto element=[&](size_t vector,size_t i){const auto at=vector+4+4*i;return at+u32(at);};
        const auto bones=object(element(object(object(u32(0),3),4),0),2);
        for(size_t bone=1;bone<3;++bone) { // the constant tracks become full-length aliases of track 0's samples
            const auto track=element(object(element(bones,bone),2),0);
            Put<uint32_t>(shared,field(track,3),1);Put<uint32_t>(shared,field(track,4),frames);
            Put<uint32_t>(shared,field(track,6),0);Put<uint32_t>(shared,field(track,7),frames*44);
        }
        std::string error;
        try { P::DecodeRaw(shared); } catch(const std::exception& e) { error=e.what(); }
        Check(error.find("Decoded PANM samples exceed")!=std::string::npos,("Aliased PANM tracks decoded past the budget: "+error).c_str());
    }
    auto invalid=animation;invalid.tracks.push_back(invalid.tracks.front());Reject([&]{P::Encode(invalid);});
    invalid=animation;invalid.tracks[0].samples.pop_back();Reject([&]{P::Encode(invalid);});
    invalid=animation;invalid.tracks[0].samples[0].rotation.fill(0);Reject([&]{P::Encode(invalid);});
    invalid=animation;invalid.tracks[1].samples[0].translation[0]=std::numeric_limits<float>::infinity();Reject([&]{P::Encode(invalid);});
    invalid=animation;invalid.frames=0;Reject([&]{P::Encode(invalid);});
    std::vector<Byte> c8(12+2*3*4);
    Put<uint16_t>(c8,0,0xc8);Put<uint16_t>(c8,2,1);Put<uint32_t>(c8,4,2);Put<uint32_t>(c8,8,11);
    const float samples[]={1,2,3,4,5,6};memcpy(c8.data()+12,samples,sizeof(samples));
    {
        // QA 1.0 (T7DecodeFrameBudget): a 0x64 animation longer than its conversion can hold is refused before
        // the per-frame decode (whose cost grows with frames squared), and the bound rejects nothing convertible.
        const auto limit=T7AnimToRetailT8::MaxConvertibleFrames();
        Check(limit==(32u<<20)/(23*sizeof(P::Sample)),"Converted bone count changed: review MaxConvertibleFrames"); // 23 with HARA_ROT1 (#178)
        const uint16_t frames=40000,bones=1;const size_t chunks=(frames+14)/16;
        std::vector<Byte> long64(4+2*bones+6+6*bones+chunks*4+3*bones);
        Put<uint16_t>(long64,0,0x64);Put<uint16_t>(long64,2,bones);Put<uint16_t>(long64,4,4);Put<uint16_t>(long64,6,frames);
        const size_t keyframes=4+2*bones+6+6*bones;
        for(size_t chunk=0;chunk<chunks;++chunk)Put<uint32_t>(long64,keyframes+chunk*4,static_cast<uint32_t>(chunks*4));
        for(size_t channel=0;channel<3*bones;++channel)long64[keyframes+chunks*4+channel]=4;
        Check(TAnimUtils::ReadAnimationLayout(long64).frames==frames,"Long T7 animation fixture is invalid");
        std::string error;const auto start=std::chrono::steady_clock::now();
        try { P::DecodeT7(long64,limit); } catch(const std::exception& e) { error=e.what(); }
        Check(error.find("more frames than its conversion can hold")!=std::string::npos,("Overlong T7 animation was decoded: "+error).c_str());
        Check(std::chrono::steady_clock::now()-start<std::chrono::seconds(1),"Overlong T7 animation was refused only after decoding");
    }
    const auto source=c8;auto t7=P::DecodeT7(c8);
    Check(t7.frames==2 && t7.channels==1 && t7.values==std::vector<float>(samples,samples+6) && source==c8,"T7 frame decoding changed source or sample order");
    Put<float>(c8,12,std::numeric_limits<float>::quiet_NaN());Reject([&]{P::DecodeT7(c8);});
    std::vector<Byte> c64(18);
    Put<uint16_t>(c64,0,0x64);Put<uint16_t>(c64,2,1);Put<uint16_t>(c64,4,7);Put<uint16_t>(c64,6,1);
    t7=P::DecodeT7(c64);Check(t7.frames==1 && t7.channels==1 && t7.values==std::vector<float>(3),"T7 compressed initial pose did not decode");
    c64.pop_back();Reject([&]{P::DecodeT7(c64);});
    RunCompiledAnimationTests();
    RunPanmToT7Tests();
}
