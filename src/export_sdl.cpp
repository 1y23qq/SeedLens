#include "export.h"
#include <codecvt>
#include <locale>
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

namespace seedlens {
using UtfCodec=std::conditional_t<sizeof(wchar_t)==2,std::codecvt_utf8_utf16<wchar_t>,std::codecvt_utf8<wchar_t>>;
std::string utf8(const std::wstring& s){return std::wstring_convert<UtfCodec>().to_bytes(s);}
std::wstring wide(const std::string& s){return std::wstring_convert<UtfCodec>().from_bytes(s);}
bool read_png_resource(int id,std::vector<uint32_t>& pixels,int& width,int& height){
    auto bytes=embedded_resource(id);int channels=0;
    auto* data=stbi_load_from_memory(bytes.data(),int(bytes.size()),&width,&height,&channels,4);
    if(!data)return false;
    pixels.resize(size_t(width)*height);memcpy(pixels.data(),data,pixels.size()*4);
    stbi_image_free(data);return true;
}
bool write_png(const std::filesystem::path& path,int width,int height,const uint32_t* rgba,std::string& error){
    if(width<1||height<1||width>8192||height>8192){error="Invalid image dimensions.";return false;}
    bool ok=stbi_write_png(path.string().c_str(),width,height,4,rgba,width*4)!=0;
    error=ok?"":"PNG export failed.";return ok;
}
}
