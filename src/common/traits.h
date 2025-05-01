#pragma once

#include "proxy/proxy.h"

#include "common.h"

struct Ray;
struct Interval;
struct AABB;
struct HitRecord;
struct ScatterRecord;


// Hittable

PRO_DEF_MEM_DISPATCH(HittableMemHit, hit);
PRO_DEF_MEM_DISPATCH(HittableMemBB, bounding_box);
PRO_DEF_MEM_DISPATCH(HittableMemPDFValue, pdf_value);
PRO_DEF_MEM_DISPATCH(HittableMemRandom, random);

struct Hittable                                         //
    : pro::facade_builder                               //
      ::support_copy<pro::constraint_level::nontrivial> //
      ::add_convention<HittableMemHit,
          bool(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const> //
      ::add_convention<HittableMemBB, AABB() const>                              //
      ::add_convention<HittableMemPDFValue,
          double(const Vec3d& origin, const Vec3d& direction) const>        //
      ::add_convention<HittableMemRandom, Vec3d(const Vec3d& origin) const> //
      ::build {};

// Material

PRO_DEF_MEM_DISPATCH(MaterialMemEmitted, emitted);
PRO_DEF_MEM_DISPATCH(MaterialMemScatter, scatter);
PRO_DEF_MEM_DISPATCH(MaterialMemScatteringPDF, scattering_pdf);

struct Material                                         //
    : pro::facade_builder                               //
      ::support_copy<pro::constraint_level::nontrivial> //
      ::add_convention<MaterialMemEmitted,
          Vec3d(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
              const Vec3d& p) const> //
      ::add_convention<MaterialMemScatter,
          bool(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const> //
      ::add_convention<MaterialMemScatteringPDF,
          double(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const> //
      ::build {};


// Texture


PRO_DEF_MEM_DISPATCH(TextureMemValue, value);

struct Texture                                          //
    : pro::facade_builder                               //
      ::support_copy<pro::constraint_level::nontrivial> //
      ::add_convention<TextureMemValue,
          Vec3d(const double u, const double v, const Vec3d& p) const> //
      ::build {};


// PDF

PRO_DEF_MEM_DISPATCH(PDFMemValue, value);
PRO_DEF_MEM_DISPATCH(PDFMemGenerate, generate);

struct PDF                                                                //
    : pro::facade_builder                                                 //
      ::support_copy<pro::constraint_level::nontrivial>                   //
      ::add_convention<PDFMemValue, double(const Vec3d& direction) const> //
      ::add_convention<PDFMemGenerate, Vec3d() const>                     //
      ::build {};
