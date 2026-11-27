#include "RetailPanm.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Importer_t8/RetailResourceLoader.hpp"
#include "T7AnimToRetailT8.hpp"
#include "T7HandToRetailT8.hpp"
#include "T7CameraToRetailT8.hpp"
#include "T7WingToRetailT8.hpp"
#include "T7FaceToRetailT8.hpp"
#include "RetailT8AnimToT7.hpp"
#include <numbers>
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
        // LowerBody at its rest (T7 down loops such as Mco_dwkam0): HARA_ROT1 still carries FullBody, as native T8 does.
        const auto restLower=body(false,true);
        const auto& restHara=track(restLower,"HARA_ROT1");
        Check(restHara.samples.size()==6,"HARA_ROT1 is not animated with LowerBody at rest");
        for(size_t f=0;f<6;++f)
            Check(sameRotation(restHara.samples[f],f>=3?faceUp:identity),"HARA_ROT1 does not carry FullBody with LowerBody at rest");
    }
}
// T7 hand MOTA animation -> retail hand PANM (T7HandToRetailT8; verified against native T8 Kazuya hands).
void RunHandTests() {
    namespace P=RetailPanm;namespace H=T7HandToRetailT8;
    // 19 channels, 2 frames: channel 1 (T8 Thumb0) has axis-angle (a, b, c) = (0.3, 0.2, 0.5) on frame 1, all else 0.
    P::T7Animation source{2,19,std::vector<float>(2*19*3,0.0f),true};
    const double a=0.3,b=0.2,c=0.5;
    source.values[(19+1)*3+0]=float(a);source.values[(19+1)*3+1]=float(b);source.values[(19+1)*3+2]=float(c);
    const double s=std::sin(c/2);
    const std::array<double,4> right{-std::cos(a)*std::sin(b)*s,std::sin(a)*s,-std::cos(a)*std::cos(b)*s,std::cos(c/2)};
    auto rotation=[&](const P::Animation& out,const std::string& bone,uint32_t frame){
        for(const auto& t:out.tracks)if(t.name==bone)return t.samples.size()==1?t.samples[0]:t.samples.at(frame);
        throw std::runtime_error("Converted hand lacks a bone");
    };
    for(auto side:{H::Side::Right,H::Side::Left}){
        const auto out=P::Decode(H::Convert(source,side));
        const std::string prefix=side==H::Side::Left?"L_":"R_";
        Check(out.frames==2 && out.tracks.size()==19,"Converted hand has another skeleton");
        for(const auto& t:out.tracks)Check(t.name.rfind(prefix,0)==0 && t.options.empty() && t.samples.size()==2,"Converted hand bone, options or length changed");
        const auto sample=rotation(out,prefix+"Thumb0",1);
        const double mirror=side==H::Side::Left?-1:1;
        Check(std::abs(sample.rotation[0]-mirror*right[0])<1e-6 && std::abs(sample.rotation[1]-mirror*right[1])<1e-6 &&
            std::abs(sample.rotation[2]-right[2])<1e-6 && std::abs(sample.rotation[3]-right[3])<1e-6,"Hand rotation is not the (mirrored) inverse axis-angle");
        const auto rest=rotation(out,prefix+"Index1",0);
        Check(rest.rotation==std::array<float,4>{0,0,0,1} && rest.translation[0]>10 && rest.scale==std::array<float,3>{1,1,1},
            "Hand bones lost their native rest offsets");
    }
    // 15 channels: no metacarpals; Index0/Middle0/Ring0/Pinky0 keep the native rest pose (static tracks).
    P::T7Animation fifteen{3,15,std::vector<float>(3*15*3,0.1f),true};
    const auto out=P::Decode(H::Convert(fifteen,H::Side::Right));
    size_t statics=0;for(const auto& t:out.tracks)statics+=t.samples.size()==1;
    Check(out.tracks.size()==19 && statics==4 && rotation(out,"R_Index0",0).rotation==std::array<float,4>{0,0,0,1},"15-channel hand mapping changed");
    bool refused=false;try{H::Convert(P::T7Animation{2,23,std::vector<float>(2*23*3),true},H::Side::Right);}catch(const std::invalid_argument&){refused=true;}
    Check(refused,"A body animation was converted as a hand");
    // MOTA archives: little- and big-endian offset lists, bounds checks.
    std::vector<Byte> mota(0x40,0);memcpy(mota.data(),"MOTA",4);mota[0x11]=1;
    auto put=[&](size_t at,uint32_t v,bool little){for(int i=0;i<4;++i)mota[at+i]=Byte(little?(v>>(8*i)):(v>>(8*(3-i))));};
    put(0x0C,2,true);put(0x14,0x20,true);put(0x18,0x30,true);
    auto list=H::MotaAnimations(mota);
    Check(list.size()==2 && list[0].data()==mota.data()+0x20 && list[1].data()==mota.data()+0x30 && list[1].size()==0x10,"MOTA animation list changed");
    mota[0x11]=0;put(0x0C,2,false);put(0x14,0x20,false);put(0x18,0x30,false);
    Check(H::MotaAnimations(mota).size()==2 && H::MotaAnimations(mota)[1].data()==mota.data()+0x30,"Big-endian MOTA list misread");
    put(0x18,0x40,false);
    refused=false;try{H::MotaAnimations(mota);}catch(const std::invalid_argument&){refused=true;}
    Check(refused,"MOTA animation past the archive accepted");
    put(0x0C,0x1000,false);
    refused=false;try{H::MotaAnimations(mota);}catch(const std::invalid_argument&){refused=true;}
    Check(refused,"MOTA count past the archive accepted");
    Check(H::FindT7Property(0x842E)->retail==0x8610 && H::FindT7Property(0x842F)->side==H::Side::Right && !H::FindT7Property(0x842D) &&
        H::FindRetailProperty(0x8611)->t7==0x842F && H::AssetIndex(H::AssetInstance(0x2abc))==0x2abc,"Hand property table changed");
}
}
// Native camera PANM layout (EncodeGroups, packed codecs) and T7 camera MOTA conversion (T7CameraToRetailT8).
std::vector<RetailPanm::TrackOption> Sorted(std::vector<RetailPanm::TrackOption> o) {
    std::sort(o.begin(),o.end(),[](const auto& a,const auto& b){ return a.field<b.field; });return o;
}
void RunCameraTests() {
    namespace P=RetailPanm;namespace C=T7CameraToRetailT8;
    // Packed transforms: animated parts within their steps, constant channels take no bits, sign of w kept.
    std::vector<P::Sample> samples(50);
    for(size_t i=0;i<samples.size();++i){
        const double a=0.07*double(i)-1.5;
        samples[i].rotation={float(std::sin(a/2)*0.6),float(std::sin(a/2)*0.8),0.0f,float(std::cos(a/2))};
        samples[i].translation={float(i)*0.5f,100.0f,-3.0f*float(i)};
    }
    samples[30].rotation={-samples[30].rotation[0],-samples[30].rotation[1],-samples[30].rotation[2],-samples[30].rotation[3]}; // w < 0
    const auto packed=P::PackTransforms(samples);
    uint16_t flags;memcpy(&flags,packed.data()+2,2);
    Check(flags==14,"Packed camera transform flags differ from native (rotation and position animated)");
    // Listed widths (issue #182): a constant channel of an animated group lists 0 like native files (rotation z, position
    // y here); only the groups the track does not animate (scale) list a nominal 16.
    auto width=[&](size_t c){uint32_t w;memcpy(&w,packed.data()+24+c*16,4);return w;};
    Check(width(0)==16 && width(1)==16 && width(2)==16,"Unanimated scale channels do not list the native nominal width");
    Check(width(5)==0 && width(7)==0,"Constant channel of an animated group lists a width (issue #182)");
    Check(width(3)>0 && width(4)>0 && width(6)>0 && width(8)>0,"Animated channel lists no width");
    const auto back=P::DecodePackedTrack(packed,uint32_t(samples.size()));
    for(size_t i=0;i<samples.size();++i){
        for(int k=0;k<4;++k)Check(std::abs(back[i].rotation[k]-samples[i].rotation[k])<2e-4f,"Packed rotation drifted (or lost the sign of w)");
        for(int k=0;k<3;++k)Check(std::abs(back[i].translation[k]-samples[i].translation[k])<1e-3f && back[i].scale[k]==1.0f,"Packed position or scale drifted");
    }
    // A constant rotation with w < 0 is stored as its w >= 0 equivalent (the stream has no sign bit for it).
    std::vector<P::Sample> still(3);for(auto& s:still)s.rotation={0,0,-0.6f,-0.8f};
    const auto stillBack=P::DecodePackedTrack(P::PackTransforms(still),3);
    Check(std::abs(stillBack[1].rotation[2]-0.6f)<1e-6f && std::abs(stillBack[1].rotation[3]-0.8f)<1e-6f,"Constant rotation lost its orientation");
    std::vector<float> fov{0.4f,0.45f,0.5f,0.5f};
    const auto fovBack=P::UnpackScalars(P::PackScalars(fov),4);
    for(size_t i=0;i<fov.size();++i)Check(std::abs(fovBack[i]-fov[i])<2e-5f,"Packed scalar drifted");
    Check(P::UnpackScalars(P::PackScalars(std::vector<float>{0.3f,0.3f}),2)==std::vector<float>{0.3f,0.3f},"Constant scalar changed");
    // Groups: decode gives back the tracks verbatim; groups keep first-seen order; data 16-aligned.
    std::vector<P::GroupTrack> tracks(3);
    tracks[0]={1,"cam_transform","Transform",1,3,50,{{3,1},{4,1}},packed};
    tracks[1]={5,"cam_transform","FieldOfView",3,2,1,{},std::vector<Byte>(4,0x3e)};
    tracks[2]={1,"Camera_Param","Transform",1,3,50,{{1,1},{2,1},{4,1}},P::PackTransforms(std::vector<P::Sample>(50))};
    uint32_t frames=0;const auto decoded=P::DecodeGroups(P::EncodeGroups(50,tracks),&frames);
    Check(frames==50 && decoded.size()==3 && decoded[0].bone=="cam_transform" && decoded[1].bone=="Camera_Param" && decoded[1].group==1 &&
        decoded[2].group==5 && decoded[2].name=="FieldOfView" && decoded[2].type==3 && decoded[2].encoding==2 && decoded[0].data==packed &&
        Sorted(decoded[0].options)==Sorted(tracks[0].options) && decoded[2].data==tracks[1].data,"Grouped PANM round trip changed");
    // T7 clips: 0xC8, u16 channels, u32 frames, u32 descriptors, then frames x channels x 3 floats.
    auto clip=[](std::vector<uint32_t> desc,std::vector<std::vector<std::array<float,3>>> frames,bool big){
        std::vector<Byte> b;auto u16=[&](uint16_t v){for(int i=0;i<2;++i)b.push_back(Byte(big?v>>(8*(1-i)):v>>(8*i)));};
        auto u32=[&](uint32_t v){for(int i=0;i<4;++i)b.push_back(Byte(big?v>>(8*(3-i)):v>>(8*i)));};
        u16(0xC8);u16(uint16_t(desc.size()));u32(uint32_t(frames.size()));for(auto d:desc)u32(d);
        for(const auto& f:frames)for(const auto& ch:f)for(float v:ch){uint32_t r;memcpy(&r,&v,4);u32(r);}
        return b;
    };
    auto track=[](const std::vector<P::GroupTrack>& t,const std::string& bone,const std::string& name)->const P::GroupTrack&{
        for(const auto& x:t)if(x.bone==bone && x.name==name)return x;
        throw std::runtime_error("Converted camera lacks a track");
    };
    // Euler layout: position mm -> cm, q = Rz Ry Rx, FOV = zoom x 9/16 (constant: static track), no Camera_Param.
    for(bool big:{false,true}){
        const auto bytes=clip({C::Position,C::Rotation,C::Zoom,C::Roll},
            {{{100,200,300},{0.1f,0.2f,0.3f},{1,0,0},{0,0,0}},{{110,200,300},{0.1f,0.2f,0.3f},{1,0,0},{0,0,0}}},big);
        const auto camera=C::Read(bytes);
        Check(camera.frames==2 && camera.descriptors.size()==4 && C::Convertible(camera),"T7 camera clip misread");
        const auto out=P::DecodeGroups(C::Convert(camera));
        Check(out.size()==2,"Euler camera has a Camera_Param track");
        const auto& t=track(out,"cam_transform","Transform");const auto& f=track(out,"cam_transform","FieldOfView");
        Check(t.group==1 && f.group==5 && f.type==3 && f.encoding==2 && Sorted(t.options)==Sorted({{3,1},{4,1}}),"Camera tracks differ from the native layout");
        float v;memcpy(&v,f.data.data(),4);Check(std::abs(v-0.5625f)<1e-6f,"FOV is not zoom x 9/16");
        const auto s=P::DecodePackedTrack(t.data,2);
        const double cx=std::cos(0.05),sx=std::sin(0.05),cy=std::cos(0.1),sy=std::sin(0.1),cz=std::cos(0.15),sz=std::sin(0.15);
        const std::array<double,4> q{cz*cy*sx-sz*sy*cx,cz*sy*cx+sz*cy*sx,sz*cy*cx-cz*sy*sx,cz*cy*cx+sz*sy*sx};
        for(int k=0;k<4;++k)Check(std::abs(s[0].rotation[k]-q[k])<1e-5,"Euler rotation is not Rz*Ry*Rx");
        Check(std::abs(s[0].translation[0]-10)<1e-3f && std::abs(s[1].translation[0]-11)<1e-3f && std::abs(s[0].translation[2]-30)<1e-3f,"Camera position is not mm/10");
    }
    // Look-at layout: target, camera; roll 90 degrees about world Z before the look-at rotation; smoothing -> Camera_Param.
    const auto lookAt=C::Convert(C::Read(clip({C::Position,C::Position,C::Zoom,C::Smoothing,C::Roll},
        {{{0,0,-1000},{0,0,0},{1,0,0},{1,0.1f,1},{90,0,0}},{{0,0,-1000},{0,0,0},{0.8f,0,0},{1,0.1f,1},{0,0,0}}},false)));
    const auto out=P::DecodeGroups(lookAt);
    const auto look=P::DecodePackedTrack(track(out,"cam_transform","Transform").data,2);
    Check(std::abs(look[1].rotation[3]-1)<1e-5 && std::abs(look[0].rotation[2]-std::sin(std::numbers::pi/4))<1e-4 &&
        std::abs(look[0].rotation[3]-std::cos(std::numbers::pi/4))<1e-4,"Look-at rotation or degree roll changed");
    const auto params=P::DecodePackedTrack(track(out,"Camera_Param","Transform").data,2);
    Check(std::abs(params[0].scale[1]-0.1f)<1e-3f && params[0].rotation[3]==1,"Smoothing is not in Camera_Param's scale");
    Check(track(out,"cam_transform","FieldOfView").encoding==3,"Animated FOV is not packed");
    bool refused=false;try{C::Read(std::vector<Byte>{0x64,0,1,0,2,0,0,0});}catch(const std::invalid_argument&){refused=true;}
    Check(refused,"Keyframed T7 camera clip accepted");
    Check(C::FindT7Property(33845)->retail==0x868C && C::FindT7Property(33845)->paired==0x868D && !C::FindRetailProperty(0x8688) && C::FindRetailProperty(0x868D)->t7==33850 && !C::FindT7Property(33852) &&
        C::AssetIndex(C::AssetInstance(0x2001))==0x2001,"Camera property table changed");
}
// Packed tracks with a uniform scale (flag 1, only scale x in the stream) and T7 wing MOTA conversion (T7WingToRetailT8).
void RunWingTests() {
    namespace P=RetailPanm;namespace W=T7WingToRetailT8;
    // Uniform scale: x, y and z listed 1..2 with 8 bits each, 8 bits per frame (x only), 3 frames 0, 128, 255.
    std::vector<Byte> track(16+9*16+44+3,0);
    auto put32=[&](size_t at,uint32_t v){memcpy(track.data()+at,&v,4);};auto putf=[&](size_t at,float v){memcpy(track.data()+at,&v,4);};
    uint16_t h[4]={4,1,160,8};memcpy(track.data(),h,8);put32(8,204);put32(12,3);
    for(int c=0;c<9;++c){putf(16+c*16,c<3?1.0f:c==6?0.0f:0.0f);putf(20+c*16,c<3?2.0f:0.0f);put32(24+c*16,c<3?8:16);}
    P::Sample base;memcpy(track.data()+160,&base,sizeof(base));
    track[204]=0;track[205]=128;track[206]=255;
    const auto uniform=P::DecodePackedTrack(track,3);
    Check(uniform[2].scale==std::array<float,3>{2,2,2} && uniform[0].scale==std::array<float,3>{1,1,1} &&
        std::abs(uniform[1].scale[1]-(1+128.0f/255))<1e-5f && uniform[1].scale[1]==uniform[1].scale[0] && uniform[1].rotation[3]==1,
        "Uniform packed scale misread");
    const uint16_t odd=12;memcpy(track.data()+6,&odd,2);track.resize(track.size()+3,0); // neither 24/25 nor 8/9 bits
    bool refused=false;try{P::DecodePackedTrack(track,3);}catch(const std::exception&){refused=true;}
    Check(refused,"Packed track with an unexplained bit count accepted");
    // Wings: a 0xC8 clip of 6 bones (position, rotation) at the eye rig's rest offsets x 10 (mm).
    const float rest[6][3]={{14,8.70568f,0},{14,8.70568f,0.01f},{14,8.70568f,-0.01f},{10.002f,0,9.55463f},{10.002f,0.01f,9.55463f},{10.002f,-0.01f,9.55463f}};
    auto clip=[&](float offset){
        std::vector<Byte> b;auto u16=[&](uint16_t v){b.push_back(Byte(v));b.push_back(Byte(v>>8));};
        auto u32=[&](uint32_t v){for(int i=0;i<4;++i)b.push_back(Byte(v>>(8*i)));};auto f32=[&](float v){uint32_t r;memcpy(&r,&v,4);u32(r);};
        u16(0xC8);u16(12);u32(2);for(int i=0;i<6;++i){u32(0x0B);u32(0x07);}
        for(int f=0;f<2;++f)for(int i=0;i<6;++i){for(int k=0;k<3;++k)f32(rest[i][k]*10+(k==0?offset:0));f32(f*0.2f);f32(0);f32(0);}
        return b;
    };
    std::string rig;
    const auto out=P::Decode(W::Convert(clip(0),&rig));
    Check(rig=="com/swing_9AD06629.panm" && out.tracks.size()==6 && out.tracks[0].name=="ATH_Spine2_eye" && out.tracks[5].name=="ATH_Head_Reyelid" &&
        out.tracks[0].options.empty() && std::abs(out.tracks[3].samples[0].translation[2]-9.55463f)<1e-4f,"Wing bones or rest offsets changed");
    const auto& r=out.tracks[1].samples[1].rotation; // ZYX Euler (0.2, 0, 0) -> x rotation
    Check(std::abs(r[0]-std::sin(0.1))<1e-5 && std::abs(r[3]-std::cos(0.1))<1e-5 && out.tracks[1].samples[1].scale==std::array<float,3>{1,1,1},
        "Wing rotation or default scale changed");
    refused=false;try{W::Convert(clip(5));}catch(const std::invalid_argument&){refused=true;} // 0.5 cm off the rig
    Check(refused,"Wing animation off every rig's rest offsets converted");
    Check(W::FindT7Property(33840)->retail==0x8683 && W::FindRetailProperty(0x8683)->t7==33840 && W::AssetIndex(W::AssetInstance(0x2003))==0x2003,
        "Wing property table changed");
}
// T7 face MOTA conversion onto the legacy 43-bone face rig (T7FaceToRetailT8).
void RunFaceTests() {
    namespace P=RetailPanm;namespace F=T7FaceToRetailT8;
    // 0xC8 clip: 43 x (position, rotation) then four scale channels; bone 33 (Jaw_Joint) opens on frame 1.
    auto clip=[](std::vector<uint32_t> desc){
        std::vector<Byte> b;auto u16=[&](uint16_t v){b.push_back(Byte(v));b.push_back(Byte(v>>8));};
        auto u32=[&](uint32_t v){for(int i=0;i<4;++i)b.push_back(Byte(v>>(8*i)));};auto f32=[&](float v){uint32_t r;memcpy(&r,&v,4);u32(r);};
        u16(0xC8);u16(uint16_t(desc.size()));u32(2);for(auto d:desc)u32(d);
        for(int f=0;f<2;++f)for(size_t c=0;c<desc.size();++c){
            if(desc[c]==0x0B){f32(float(c)*10);f32(20);f32(30);}
            else if(desc[c]==0x07){f32(c==67 && f==1?0.3f:0.0f);f32(0);f32(0);}
            else{f32(1);f32(1);f32(1);}
        }
        return b;
    };
    std::vector<uint32_t> desc;for(int k=0;k<43;++k){desc.push_back(0x0B);desc.push_back(0x07);}for(int k=0;k<4;++k)desc.push_back(0x03);
    const auto bytes=F::Convert(clip(desc));
    uint32_t frames=0;const auto tracks=P::DecodeGroups(bytes,&frames);
    Check(frames==2 && tracks.size()==43 && tracks[0].bone=="L_Mayu1_Joint" && tracks[33].bone=="Jaw_Joint" && tracks[42].bone=="Bero3_Joint" &&
        Sorted(tracks[0].options)==Sorted({{3,1},{4,1}}),"Face bones or options changed");
    Check(tracks[0].encoding==2 && tracks[0].samples==1 && tracks[33].encoding==3 && tracks[33].samples==2,"Still and moving face bones are not static and packed");
    P::Sample still;memcpy(&still,tracks[0].data.data(),sizeof(still));
    Check(std::abs(still.translation[0])<1e-6f && std::abs(still.translation[1]-2)<1e-6f && still.rotation[3]==1,"Face position is not mm/10");
    const auto jaw=P::DecodePackedTrack(tracks[33].data,2);
    Check(std::abs(jaw[1].rotation[0]-std::sin(0.15))<1e-5 && std::abs(jaw[1].translation[0]-66)<1e-3f,"Face rotation (inverse stored as Euler) changed");
    for(auto bad:{std::vector<uint32_t>(70,0x07),std::vector<uint32_t>(desc.begin(),desc.end()-1)}){
        bool refused=false;try{F::Convert(clip(bad));}catch(const std::invalid_argument&){refused=true;}
        Check(refused,"Face animation of another layout converted");
    }
    Check(F::FindT7Property(33986)->retail==0x877A && F::FindRetailProperty(0x877A)->t7==33986 && F::AssetIndex(F::AssetInstance(0x2004))==0x2004 &&
        F::AssetInstance(0x2000)!=T7HandToRetailT8::AssetInstance(0x2000),"Face property table or instances changed");
}
void RunRetailPanmTests() {
    namespace P=RetailPanm;
    RunHandTests();
    RunCameraTests();
    RunWingTests();
    RunFaceTests();
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
