/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <iosfwd>
#include <string>
#include <sutil.h>


//-----------------------------------------------------------------------------
//
// Utility functions
//
//-----------------------------------------------------------------------------

// Creates a TextureSampler object for the given PPM file.  If filename is
// empty or PPMLoader fails, a 1x1 texture is created with the provided default
// texture color.
SUTILAPI sutil::Texture loadPPMTexture( const std::string& ppm_filename, const float3& default_color, cudaTextureDesc* tex_desc );

//-----------------------------------------------------------------------------
//
// PPMLoader class declaration
//
//-----------------------------------------------------------------------------

class PPMLoader
{
  public:
    SUTILAPI PPMLoader( const std::string& filename, const bool vflip = false );
    SUTILAPI ~PPMLoader();

    SUTILAPI sutil::Texture loadTexture( const float3& default_color, cudaTextureDesc* tex_desc );

    SUTILAPI bool           failed() const;
    SUTILAPI unsigned int   width() const;
    SUTILAPI unsigned int   height() const;
    SUTILAPI unsigned char* raster() const;

  private:
    unsigned int   m_nx;
    unsigned int   m_ny;
    unsigned int   m_max_val;
    unsigned char* m_raster;
    bool           m_is_ascii;

    static void getLine( std::ifstream& file_in, std::string& s );
};
