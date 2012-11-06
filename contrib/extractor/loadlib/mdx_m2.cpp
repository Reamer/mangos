#include "mdx_m2.h"


MDX_M2_file::MDX_M2_file()
{
    model = 0;
}

MDX_M2_file::~MDX_M2_file()
{
    free();
}

void MDX_M2_file::free()
{
    model = 0;
    FileLoader::free();
}
