#include "RetailPanm.hpp"
#include "AnimationReader.hpp"
#include "Animations.hpp"
#include <cmath>
#include <bit>
#include <cstring>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace RetailPanm {
namespace {
constexpr size_t Limit=32ULL<<20;
void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
template<class T> T Read(std::span<const Byte> b,size_t at) {
    Check(at<=b.size() && sizeof(T)<=b.size()-at,"Truncated PANM field");
    T value;memcpy(&value,b.data()+at,sizeof(value));return value;
}
// Back-to-front FlatBuffers builder, matching the tool that wrote native PANMs: the first object built ends up
// last in the buffer, identical vtables are shared, and offsets are kept as "size from the end" until Finish.
// tools/panm_rebuild.py is the same builder in Python; it reproduces native files byte for byte.
struct Builder {
    std::vector<Byte> data=std::vector<Byte>(4096);
    size_t head=data.size(),minalign=1,start=0;
    std::vector<std::pair<size_t,size_t>> fields; // (field index, location)
    std::vector<size_t> vtables;
    size_t Size() const { return data.size()-head; }
    void Reserve(size_t count) {
        Check(count<=Limit && Size()<=Limit-count,"PANM exceeds the 32 MiB budget");
        if(count<=head)return;
        const auto size=Size(),capacity=(std::max)(data.size()*2,size+count);
        std::vector<Byte> grown(capacity);memcpy(grown.data()+capacity-size,data.data()+head,size);
        data=std::move(grown);head=capacity-size;
    }
    void Bytes(const void* source,size_t count) { Reserve(count);head-=count;memcpy(data.data()+head,source,count); }
    void Pad(size_t count) { Reserve(count);head-=count;memset(data.data()+head,0,count); }
    void Prep(size_t align,size_t extra) { minalign=(std::max)(minalign,align);Pad((align-((Size()+extra)%align))%align); }
    template<class T> void Push(T value) { Bytes(&value,sizeof(value)); }
    template<class T> void Scalar(T value) { Prep(sizeof(T),0);Push(value); }
    uint32_t Refer(size_t offset) { Prep(4,0);return static_cast<uint32_t>(Size()-offset+4); }
    void StartTable() { fields.clear();start=Size(); }
    template<class T> void Add(size_t field,T value) { if(value==T{})return;Scalar(value);fields.push_back({field,Size()}); }
    void AddOffset(size_t field,size_t offset) { if(!offset)return;Push(Refer(offset));fields.push_back({field,Size()}); }
    size_t EndTable() {
        Scalar<int32_t>(0);const auto location=Size();
        size_t count=0;for(const auto& field:fields)count=(std::max)(count,field.first+1);
        std::vector<uint16_t> vtable(2+count);
        vtable[0]=static_cast<uint16_t>(4+2*count);vtable[1]=static_cast<uint16_t>(location-start);
        for(const auto& [field,at]:fields)vtable[2+field]=static_cast<uint16_t>(location-at);
        const auto bytes=vtable.size()*2;size_t use=0;
        for(auto existing:vtables)
            if(existing>=bytes && !memcmp(data.data()+data.size()-existing,vtable.data(),bytes)) { use=existing;break; }
        if(!use) { Bytes(vtable.data(),bytes);use=Size();vtables.push_back(use); }
        const int32_t relative=static_cast<int32_t>(int64_t(use)-int64_t(location));
        memcpy(data.data()+data.size()-location,&relative,4);
        fields.clear();return location;
    }
    size_t String(const std::string& text) {
        Prep(4,text.size()+1);Pad(1);Bytes(text.data(),text.size());Scalar(static_cast<uint32_t>(text.size()));return Size();
    }
    size_t Vector(const std::vector<size_t>& offsets) {
        Prep(4,4*offsets.size());
        for(auto it=offsets.rbegin();it!=offsets.rend();++it)Push(Refer(*it));
        Scalar(static_cast<uint32_t>(offsets.size()));return Size();
    }
    std::vector<Byte> Finish(size_t root) {
        Prep(minalign,8);Bytes("PANM",4);Push(Refer(root));
        return {data.begin()+static_cast<ptrdiff_t>(head),data.end()};
    }
};
void ValidateSample(const Sample& sample) {
    for(float value:std::bit_cast<std::array<float,11>>(sample))Check(std::isfinite(value),"Nonfinite PANM transform");
    float norm=0;for(auto value:sample.rotation)norm+=value*value;
    Check(std::abs(norm-1.0f)<0.002f,"PANM quaternion is not normalized");
}
struct Reader {
    std::span<const Byte> bytes;
    size_t Field(size_t table,size_t index) const {
        const auto delta=Read<int32_t>(bytes,table);
        const auto location=int64_t(table)-delta;
        Check(location>=8 && uint64_t(location)<bytes.size(),"Invalid PANM vtable");
        const auto vt=static_cast<size_t>(location);
        const auto size=Read<uint16_t>(bytes,vt),object=Read<uint16_t>(bytes,vt+2);
        Check(size>=4 && !(size&1) && size<=1024 && vt<=bytes.size() && size<=bytes.size()-vt &&
            object>=4 && table<=bytes.size() && object<=bytes.size()-table,"Invalid PANM table extent");
        if(4+2*index>=size)return 0;
        const auto offset=Read<uint16_t>(bytes,vt+4+2*index);
        if(!offset)return 0;
        Check(offset>=4 && offset<=object-4,"Invalid PANM field offset");return table+offset;
    }
    uint32_t Value(size_t table,size_t index,uint32_t fallback=0) const { auto at=Field(table,index);return at?Read<uint32_t>(bytes,at):fallback; }
    size_t Offset(size_t field) const {
        const auto relative=Read<uint32_t>(bytes,field);
        Check(relative && field<=bytes.size() && relative<bytes.size()-field,"Invalid PANM object reference");return field+relative;
    }
    size_t Object(size_t table,size_t index) const { const auto field=Field(table,index);Check(field!=0,"Missing PANM object field");return Offset(field); }
    std::string String(size_t start) const {
        const auto size=Read<uint32_t>(bytes,start);
        Check(size && size<=256 && start+4<=bytes.size() && size<bytes.size()-(start+4) && bytes[start+4+size]==0,
            "Invalid PANM name extent");
        const auto* value=reinterpret_cast<const char*>(bytes.data()+start+4);
        Check(!memchr(value,0,size),"Embedded NUL in PANM name");return {value,size};
    }
    // Track option table: byte fields, returned in object order so a decode/encode round trip keeps the layout.
    std::vector<TrackOption> Options(size_t table) const {
        Field(table,0); // validates the vtable and object extents
        const auto vt=static_cast<size_t>(int64_t(table)-Read<int32_t>(bytes,table));
        const auto size=Read<uint16_t>(bytes,vt),object=Read<uint16_t>(bytes,vt+2);
        Check(size<=4+2*32,"Too many PANM track options");
        std::vector<std::pair<uint16_t,uint8_t>> fields;
        for(size_t index=0;4+2*index<size;++index)
            if(const auto offset=Read<uint16_t>(bytes,vt+4+2*index)) {
                Check(offset>=4 && offset<object,"Invalid PANM track option offset");
                fields.push_back({offset,static_cast<uint8_t>(index)});
            }
        std::sort(fields.begin(),fields.end());
        std::vector<TrackOption> result;
        for(const auto& [offset,field]:fields)result.push_back({field,Read<Byte>(bytes,table+offset)});
        return result;
    }
    uint32_t Count(size_t start) const {
        const auto count=Read<uint32_t>(bytes,start);
        Check(count && count<=1024 && start+4<=bytes.size() && uint64_t(count)*4<=bytes.size()-(start+4),"Invalid PANM vector extent");return count;
    }
};
}

std::vector<Byte> Encode(const Animation& animation) {
    Check(animation.frames && animation.frames<=1000000 && !animation.tracks.empty() && animation.tracks.size()<=255,
        "Invalid PANM frame or track count");
    std::set<std::string> names;size_t rawSize=0;
    for(const auto& track:animation.tracks) {
        Check(!track.name.empty() && track.name.size()<=256 && track.name.find('\0')==std::string::npos && names.insert(track.name).second,
            "Invalid or duplicate PANM bone name");
        std::set<uint8_t> fields;
        for(const auto& option:track.options)Check(option.field<32 && fields.insert(option.field).second,"Invalid PANM track option");
        Check(track.options.size()<=32,"Too many PANM track options");
        Check(track.samples.size()==1 || track.samples.size()==animation.frames,"PANM track length differs from animation");
        Check(rawSize<=Limit-4 && track.samples.size()<=(Limit-rawSize-4)/sizeof(Sample),"PANM samples exceed budget");
        rawSize+=track.samples.size()*sizeof(Sample)+4;
        for(const auto& sample:track.samples)ValidateSample(sample);
    }
    Check(rawSize<=Limit-16-8*animation.tracks.size(),"PANM samples exceed budget");
    // Raw block as in native files: each track's samples start 8-aligned after a 4-byte gap, then a 16-byte tail.
    std::vector<size_t> offsets;size_t end=0;
    for(const auto& track:animation.tracks) {
        const auto at=offsets.empty()?0:(end+4+7)&~size_t(7);
        offsets.push_back(at);end=at+track.samples.size()*sizeof(Sample);
    }
    rawSize=((end+4+7)&~size_t(7))+16;
    Builder b;std::vector<size_t> transforms,bones;
    for(size_t i=0;i<animation.tracks.size();++i) {
        const auto& track=animation.tracks[i];
        auto options=track.options;
        std::sort(options.begin(),options.end(),[](const auto& x,const auto& y){ return x.field>y.field; });
        b.StartTable();for(const auto& option:options)b.Add<uint8_t>(option.field,option.value);
        const auto flags=b.EndTable(),type=b.String("Transform");
        b.Pad(4); // native files have 4 zero bytes between each track table and its "Transform" string
        const auto count=static_cast<uint32_t>(track.samples.size());
        b.StartTable();
        b.Add<uint32_t>(8,4);b.Add<uint32_t>(7,count*static_cast<uint32_t>(sizeof(Sample)));
        b.Add<uint32_t>(6,static_cast<uint32_t>(offsets[i]));b.AddOffset(5,flags);b.Add<uint32_t>(4,count);
        b.Add<uint32_t>(3,count==1?2:1);b.Add<uint32_t>(2,1);b.AddOffset(1,type);
        transforms.push_back(b.EndTable());
    }
    for(size_t i=0;i<animation.tracks.size();++i) {
        const auto name=b.String(animation.tracks[i].name),tracks=b.Vector({transforms[i]});
        b.StartTable();b.AddOffset(2,tracks);b.AddOffset(1,name);bones.push_back(b.EndTable());
    }
    const auto boneVector=b.Vector(bones);
    b.StartTable();b.AddOffset(2,boneVector);b.Add<uint32_t>(1,1);const auto group=b.EndTable();
    b.Pad(4); // and between the mode table and the group
    b.StartTable();b.Add<uint32_t>(3,4);b.Add<float>(2,1);const auto mode=b.EndTable();
    const auto groups=b.Vector({group});
    const auto extent=static_cast<uint32_t>(rawSize);
    b.StartTable();b.AddOffset(10,mode);b.Add<uint32_t>(9,extent);b.Add<uint32_t>(7,extent);b.Add<uint32_t>(6,extent);
    b.Add<uint32_t>(5,UINT32_MAX);const auto baseField=b.Size(); // patched to the buffer size below
    b.AddOffset(4,groups);b.Add<uint32_t>(3,1);b.Add<float>(2,60);b.Add<uint32_t>(1,animation.frames-1);
    const auto anim=b.EndTable();
    b.StartTable();b.AddOffset(3,anim);b.Add<uint8_t>(2,1);const auto root=b.EndTable();
    auto bytes=b.Finish(root);
    const auto base=static_cast<uint32_t>(bytes.size());memcpy(bytes.data()+bytes.size()-baseField,&base,4);
    Check(rawSize<=Limit-bytes.size(),"PANM exceeds the 32 MiB budget");
    bytes.resize(bytes.size()+rawSize); // native raw Transform streams end with a 20-byte tail
    for(size_t i=0;i<animation.tracks.size();++i)
        memcpy(bytes.data()+base+offsets[i],animation.tracks[i].samples.data(),animation.tracks[i].samples.size()*sizeof(Sample));
    return bytes;
}

namespace {
// Encoding 3 (packed) Transform track: a 60 + 16 * channels byte header, then `samples` frames of `bitsPerFrame` bits,
// LSB first. Header: u16 version 4, u16 flags, u16 160, u16 bits per frame, u32 header size, u32 samples, then per
// channel {f32 low, f32 high, u32 width, u32 0} and the exact frame-0 Sample. The 9 channels are scale XYZ,
// quaternion XYZ and translation XYZ; a channel with low == high stores no bits whatever its width, every other
// channel stores low + (high - low) * q / (2^width - 1). One extra bit per frame (bits per frame minus the channel
// widths) is the sign of the quaternion W, which is otherwise sqrt(1 - x^2 - y^2 - z^2). Checked on 98,575 native
// tracks: frame 0 matches the stored sample to quantization precision. docs/conversion.md.
std::vector<Sample> DecodePacked(std::span<const Byte> track,uint32_t samples) {
    Check(track.size()>=60+9*16 && Read<uint16_t>(track,0)==4,"Unsupported packed PANM track version");
    const auto bitsPerFrame=Read<uint16_t>(track,6);const auto header=Read<uint32_t>(track,8);
    Check(header==60+9*16 && Read<uint32_t>(track,12)==samples,"Unsupported packed PANM track header");
    std::array<float,9> low{},range{};std::array<uint32_t,9> width{};uint32_t used=0;
    for(size_t c=0;c<9;++c) {
        low[c]=Read<float>(track,16+c*16);const auto high=Read<float>(track,20+c*16);width[c]=Read<uint32_t>(track,24+c*16);
        Check(std::isfinite(low[c]) && std::isfinite(high) && width[c]<=32,"Invalid packed PANM channel");
        if(low[c]==high)width[c]=0;
        range[c]=high-low[c];used+=width[c];
    }
    Check(bitsPerFrame==used || bitsPerFrame==used+1,"Packed PANM bit count differs from its channels");
    const bool sign=bitsPerFrame==used+1;
    Check((uint64_t(samples)*bitsPerFrame+7)/8<=track.size()-header,"Truncated packed PANM track");
    const auto data=track.subspan(header);
    auto bits=[&](uint64_t position,uint32_t count) -> uint64_t {
        uint64_t value=0;
        for(uint32_t i=0;i<count;++i,++position)value|=uint64_t((data[size_t(position>>3)]>>(position&7))&1)<<i;
        return value;
    };
    std::vector<Sample> result(samples);
    for(uint32_t frame=0;frame<samples;++frame) {
        uint64_t position=uint64_t(frame)*bitsPerFrame;std::array<float,9> value{};
        for(size_t c=0;c<9;++c) {
            value[c]=low[c];
            if(!width[c])continue;
            const auto q=bits(position,width[c]);position+=width[c];
            value[c]=static_cast<float>(low[c]+double(range[c])*double(q)/double((uint64_t(1)<<width[c])-1));
        }
        auto& out=result[frame];
        out.scale={value[0],value[1],value[2]};out.translation={value[6],value[7],value[8]};
        double x=value[3],y=value[4],z=value[5],xyz=x*x+y*y+z*z;
        if(xyz>1) { const auto n=std::sqrt(xyz);x/=n;y/=n;z/=n;xyz=1; } // quantization overshoot
        double w=std::sqrt(1-xyz);
        if(sign && bits(position,1))w=-w;
        out.rotation={float(x),float(y),float(z),float(w)};
    }
    return result;
}
Animation DecodeTracks(std::span<const Byte> bytes,bool packed);
}

Animation DecodeRaw(std::span<const Byte> bytes) { return DecodeTracks(bytes,false); }
std::vector<Sample> DecodePackedTrack(std::span<const Byte> track,uint32_t samples) { return DecodePacked(track,samples); }
Animation Decode(std::span<const Byte> bytes) { return DecodeTracks(bytes,true); }

namespace {
Animation DecodeTracks(std::span<const Byte> bytes,bool packed) {
    Check(bytes.size()>=8 && bytes.size()<=Limit && !memcmp(bytes.data()+4,"PANM",4),"Invalid PANM signature or size");
    Reader r{bytes};Animation result;
    const auto anim=r.Object(r.Offset(0),3),groups=r.Object(anim,4);
    const uint64_t frames=uint64_t(r.Value(anim,1))+1;Check(frames<=1000000,"Invalid PANM frame count");
    result.frames=static_cast<uint32_t>(frames);
    const auto base=r.Value(anim,5),extent=r.Value(anim,6);
    Check(base<=bytes.size() && extent==bytes.size()-base,"Invalid PANM sample extent");
    Check(r.Count(groups)==1,"PANM decoder requires one bone group");
    const auto group=r.Offset(groups+4),bones=r.Object(group,2);
    const auto count=r.Count(bones);
    Check(count<=255,"Too many PANM bones");std::set<std::string> names;uint64_t decoded=0;
    for(size_t i=0;i<count;++i) {
        auto bone=r.Offset(bones+4+i*4);Track output;output.name=r.String(r.Object(bone,1));
        Check(names.insert(output.name).second,"Duplicate PANM bone");
        auto tracks=r.Object(bone,2);Check(r.Count(tracks)==1,"Unsupported PANM bone track count");
        auto track=r.Offset(tracks+4);
        Check(r.String(r.Object(track,1))=="Transform" && r.Value(track,2)==1,"Unsupported PANM track type");
        const auto encoding=r.Value(track,3),samples=r.Value(track,4),offset=r.Value(track,6),size=r.Value(track,7);
        Check(encoding==1 || encoding==2 || (packed && encoding==3),"Compressed PANM track decoding is not implemented");
        Check(samples && (encoding==2?samples==1:samples==result.frames) && offset<=extent && size<=extent-offset &&
            (encoding==3 || uint64_t(samples)*sizeof(Sample)==size),"Invalid PANM Transform samples");
        // Tracks may share sample ranges: bound the decoded total like Encode does (no 255x amplification).
        decoded+=uint64_t(samples)*sizeof(Sample);Check(decoded<=Limit,"Decoded PANM samples exceed the 32 MiB budget");
        output.options=r.Options(r.Object(track,5));
        if(encoding==3)output.samples=DecodePacked(bytes.subspan(base+offset,size),samples);
        else { output.samples.resize(samples);memcpy(output.samples.data(),bytes.data()+base+offset,size); }
        for(const auto& sample:output.samples)ValidateSample(sample);
        result.tracks.push_back(std::move(output));
    }
    return result;
}

}

T7Animation DecodeT7(std::span<const Byte> bytes,uint32_t maxFrames) {
    const auto layout=TAnimUtils::ReadAnimationLayout(bytes);
    Check(layout.frames<=maxFrames,"T7 animation has more frames than its conversion can hold");
    Check(uint64_t(layout.frames)*layout.bones*3<=Limit/sizeof(float),"Decoded T7 animation exceeds budget");
    T7Animation result{layout.frames,layout.bones,std::vector<float>(size_t(layout.frames)*layout.bones*3),layout.type==0x64};
    TAnimUtils::AnimationReader reader(bytes);
    for(uint32_t frame=0;frame<layout.frames;++frame) {
        auto output=std::span<float>(result.values).subspan(size_t(frame)*layout.bones*3,layout.bones*3);
        if(layout.type==0x64)Check(TAnimUtils::FromMemory::ParseAnimation0x64(bytes,frame+1,output)==layout.bones,"Cannot decode T7 animation frame");
        else for(size_t channel=0;channel<output.size();++channel)
            output[channel]=reader.Read<float>(layout.basePose+(size_t(frame)*layout.bones*3+channel)*4,layout.swapped);
        for(float value:output)Check(std::isfinite(value),"Nonfinite T7 animation channel");
    }
    return result;
}
}
