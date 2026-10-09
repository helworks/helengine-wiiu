#include "platform/wiiu/WiiUNativeTextureUpload.hpp"
#include "platform/wiiu/WiiUGx2ReleaseQueue.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace helengine::wiiu;
/// Records actual production surface calls so allocation, failed cleanup and GPU retirement can be verified.
static unsigned Creates=0,Destroys=0,Fences=0,Locks=0,Invalidates=0,RegCalls=0;
static bool FailLock=false;
void GX2CalcSurfaceSizeAndAlignment(GX2Surface* s) {
    assert(s->tileMode==1 && s->use==1 && s->depth==1 && s->mipLevels==1);
    const unsigned code=s->format&63;
    const bool bc=code>=49&&code<=53;
    unsigned elem=bc?(code==49||code==52?8:16):s->format==0x81?1:4;
    s->pitch=bc?((s->width+3)/4+7)/8*8:(s->width+15)/16*16;
    s->alignment=256;
    s->imageSize=s->pitch*elem*(bc?(s->height+3)/4:s->height);
    if(s->format==0x81) {
        s->mipLevelOffset[0]=(s->imageSize+255)/256*256;
        s->imageSize=s->mipLevelOffset[0]+s->pitch*s->height/2;
    }
}
bool GX2RCreateSurface(GX2Surface* s,unsigned) {s->image=new std::uint8_t[s->imageSize];++Creates;return true;}
void* GX2RLockSurfaceEx(GX2Surface* s,unsigned,unsigned) {++Locks;return FailLock?nullptr:s->image;}
void GX2RUnlockSurfaceEx(GX2Surface*,unsigned,unsigned) {}
void GX2RInvalidateSurface(GX2Surface*,unsigned,unsigned) {++Invalidates;}
void GX2Invalidate(unsigned,void*,unsigned) {}
void GX2RDestroySurfaceEx(GX2Surface* s,unsigned) {assert(s->image);delete[] static_cast<std::uint8_t*>(s->image);s->image=nullptr;++Destroys;}
void GX2InitTextureRegs(GX2Texture*) {++RegCalls;}
void GX2InitSampler(GX2Sampler* s,unsigned clamp,unsigned filter) {s->Clamp=clamp;s->Filter=filter;}
void GX2DrawDone() {++Fences;}

/// Writes literal WGT1 words in byte order independent of the test host.
static void WriteWord(std::vector<std::uint8_t>& bytes,unsigned offset,unsigned value) {
    for(unsigned b=0;b<4;++b)bytes[offset+b]=static_cast<std::uint8_t>(value>>(b*8));
}
/// Verifies native byte layout, parser strictness, real upload row copies, UV views, update ordering and retirement.
int main() {
    WiiUNativeTextureLayout rgba;
    assert(TryBuildWiiUNativeTextureLayout(0x1a,3,5,3,rgba));
    assert(rgba.PitchBytes==12&&rgba.TexelLength==60&&rgba.CompMap==0x00010203);
    std::vector<std::uint8_t> payload(124,73);
    const unsigned words[]={0x31544757,1,0x1a,16,3,5,3,5,12,60,0x00010203,0,3,0,0,0};
    for(unsigned i=0;i<16;++i)WriteWord(payload,i*4,words[i]);
    WiiUNativeTextureLayout parsed;
    assert(TryParseWiiUNativeTexturePayload(payload.data(),payload.size(),nullptr,0,parsed));
    for(unsigned offset:{0u,4u,8u,12u,24u,28u,32u,36u,40u,44u,48u,52u,56u,60u}) {
        auto bad=payload;bad[offset]^=0xff;
        assert(!TryParseWiiUNativeTexturePayload(bad.data(),bad.size(),nullptr,0,parsed));
    }
    assert(!TryParseWiiUNativeTexturePayload(payload.data(),123,nullptr,0,parsed));
    assert(!TryParseWiiUNativeTexturePayload(payload.data(),124,payload.data(),1,parsed));
    assert(!TryBuildWiiUNativeTextureLayout(0xdead,3,5,0,parsed));
    assert(!TryBuildWiiUNativeTextureLayout(0x1a,8193,1,0,parsed));
    assert(!TryBuildWiiUNativeTextureLayout(0x11a,3,5,3,parsed));
    assert(!TryBuildWiiUNativeTextureLayout(0x19,3,5,3,parsed));
    assert(TryBuildWiiUNativeTextureLayout(0x19,3,5,4,parsed));

    WiiUGx2TextureHandle handle;
    InitializeWiiUNativeTextureHandle(&handle,rgba,payload.data()+64,60,false);
    assert(handle.Texture.surface.tileMode==1&&handle.Texture.surface.format==0x1a);
    assert(handle.Texture.surface.width==3&&handle.Texture.surface.height==5);
    assert(handle.Texture.surface.pitch==16&&handle.Sampler.Filter==1);
    auto* address=static_cast<std::uint8_t*>(handle.Texture.surface.image);
    for(unsigned row=0;row<5;++row) {
        assert(std::memcmp(address+row*64,payload.data()+64+row*12,12)==0);
        assert(address[row*64+12]==0);
    }
    const std::uint8_t rows[]={1,2,3,73,5,6,7,91,201,202,203,204,9,10,11,128,13,14,15,129};
    const auto createsBefore=Creates;
    UpdateWiiURgbaTextureRegion(&handle,1,2,2,2,rows,sizeof(rows),12,0,true);
    assert(Fences==1&&Creates==createsBefore&&handle.Texture.surface.image==address);
    assert(std::memcmp(address+2*64+4,rows,8)==0&&std::memcmp(address+3*64+4,rows+12,8)==0);
    assert(rows[3]==73);
    const unsigned waits=Fences,locks=Locks;
    try {UpdateWiiURgbaTextureRegion(&handle,2,0,2,1,rows,sizeof(rows),8,3,false);assert(false);}catch(const std::invalid_argument&){}
    assert(Fences==waits&&Locks==locks);
    UpdateWiiURgbaTextureRegion(&handle,0,0,1,1,rows,4,4,0,false);
    assert(address[3]==255&&rows[3]==73);

    WiiUGx2ReleaseQueue<WiiUGx2TextureHandle> releases;
    releases.Add(&handle);releases.Add(&handle);
    assert(releases.Contains(&handle)&&handle.Texture.surface.image==address);
    const auto fencesBefore=Fences,destroysBefore=Destroys;
    releases.Drain([&](WiiUGx2TextureHandle* value){assert(Fences==fencesBefore+1);DestroyWiiUTextureHandleAfterFence(value);});
    assert(Destroys==destroysBefore+1&&!releases.Contains(&handle)&&!handle.Texture.surface.image);
    releases.Drain([](WiiUGx2TextureHandle*){assert(false);});assert(Fences==fencesBefore+1);

    WiiUNativeTextureLayout bc,nv12;
    assert(TryBuildWiiUNativeTextureLayout(0x31,9,9,1,bc));
    assert(bc.StorageWidth==12&&bc.StorageHeight==12&&bc.PitchBytes==24&&bc.TexelLength==72);
    std::vector<std::uint8_t> blocks(72);for(unsigned i=0;i<72;++i)blocks[i]=static_cast<std::uint8_t>(i);
    InitializeWiiUNativeTextureHandle(&handle,bc,blocks.data(),blocks.size(),true);
    address=static_cast<std::uint8_t*>(handle.Texture.surface.image);
    for(unsigned row=0;row<3;++row)assert(std::memcmp(address+row*64,blocks.data()+row*24,24)==0);
    const auto immutableFences=Fences;
    try {UpdateWiiURgbaTextureRegion(&handle,0,0,1,1,rows,4,4,3,false);assert(false);}catch(const std::invalid_argument&){}
    assert(Fences==immutableFences&&address[0]==0);
    DestroyWiiUTextureHandleAfterFence(&handle);

    assert(TryBuildWiiUNativeTextureLayout(0x81,3,5,0,nv12));
    assert(nv12.StorageWidth==4&&nv12.StorageHeight==6&&nv12.PitchBytes==4&&nv12.UvPlaneOffset==24&&nv12.TexelLength==36);
    std::vector<std::uint8_t> planes(36);for(unsigned i=0;i<36;++i)planes[i]=static_cast<std::uint8_t>(i+3);
    InitializeWiiUNativeTextureHandle(&handle,nv12,planes.data(),36,false);
    assert(handle.HasUvPlaneTexture&&handle.LogicalWidth==3&&handle.LogicalHeight==5);
    assert(handle.Texture.surface.width==4&&handle.Texture.surface.height==6);
    assert(handle.UvPlaneTexture.surface.width==2&&handle.UvPlaneTexture.surface.height==3&&handle.UvPlaneTexture.surface.pitch==8);
    assert(handle.UvPlaneTexture.surface.image==static_cast<std::uint8_t*>(handle.Texture.surface.image)+256);
    assert(handle.UvPlaneTexture.surface.format==7&&handle.UvPlaneTexture.compMap==0x00010405);
    address=static_cast<std::uint8_t*>(handle.Texture.surface.image);
    for(unsigned row=0;row<6;++row)assert(std::memcmp(address+row*16,planes.data()+row*4,4)==0);
    for(unsigned row=0;row<3;++row)assert(std::memcmp(address+256+row*16,planes.data()+24+row*4,4)==0);
    DestroyWiiUTextureHandleAfterFence(&handle);
    assert(!handle.HasUvPlaneTexture&&!handle.UvPlaneTexture.surface.image);
    FailLock=true;
    try {InitializeWiiUNativeTextureHandle(&handle,rgba,payload.data()+64,60,false);assert(false);}catch(const std::runtime_error&){}
    assert(!handle.Texture.surface.image&&Creates==Destroys);
    assert(RegCalls>=4&&Invalidates>=4);
    std::puts("Wii U WGT1 strict layout, native/BC/NV12 uploads, region updates and fenced retirement passed.");
}
