#include "image_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace bench {
bool loadImage(const std::filesystem::path& path, ImageBuffer& output, std::string& error)
{
    int w=0,h=0,n=0; auto* data=stbi_load(path.string().c_str(),&w,&h,&n,3);
    if(!data){error=stbi_failure_reason()?stbi_failure_reason():"stb_image failed";return false;}
    output.width=w;output.height=h;output.stride=w*3;
    output.rgb.assign(data,data+static_cast<std::size_t>(output.stride)*h);stbi_image_free(data);return true;
}
bool probeImage(const std::filesystem::path& path, int& width, int& height, std::string& error)
{
    int channels=0;
    if(stbi_info(path.string().c_str(),&width,&height,&channels))return true;
    error=stbi_failure_reason()?stbi_failure_reason():"stb_image probe failed";return false;
}
}
