#include <vector>
#define main imgui_font_helper_main
#include "../third_party/imgui/misc/fonts/binary_to_compressed_c.cpp"
#undef main
int main(int argc,char** argv){
    if(argc!=3)return 1;
    FILE* input=fopen(argv[1],"rb");
    if(!input)return 1;
    fseek(input,0,SEEK_END);long length=ftell(input);rewind(input);
    if(length<=0||length>32000000){fclose(input);return 1;}
    std::vector<unsigned char> bytes(size_t(length)+4),packed(size_t(length)*2+1024);
    if(fread(bytes.data(),1,size_t(length),input)!=size_t(length)){fclose(input);return 1;}
    fclose(input);
    unsigned size=stb_compress(packed.data(),bytes.data(),unsigned(length));
    FILE* output=fopen(argv[2],"wb");
    if(!output)return 1;
    bool ok=fwrite(packed.data(),1,size,output)==size;
    fclose(output);return ok?0:1;
}
