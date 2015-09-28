#include "audiochunks.h"
#include <string.h>
#include <algorithm>
#include "errorhandling.h"
#include "xmlconfig.h"

using namespace TASCAR;

wave_t::wave_t(uint32_t chunksize)
  : d(new float[std::max(1u,chunksize)]),
    n(chunksize), own_pointer(true),
    append_pos(0)
{
  clear();
}


wave_t::wave_t(uint32_t chunksize,float* ptr)
  : d(ptr), n(chunksize), own_pointer(false), append_pos(0)
{
}

wave_t::wave_t(const wave_t& src)
  : d(new float[std::max(1u,src.n)]),
    n(src.n), own_pointer(true), append_pos(src.append_pos)
{
  for(uint32_t k=0;k<n;k++)
    d[k] = src.d[k];
}

wave_t::~wave_t()
{
  if( own_pointer )
    delete [] d;
}

void wave_t::clear()
{
  memset(d,0,sizeof(float)*n);
}

uint32_t wave_t::copy(float* data,uint32_t cnt,float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;k++)
    d[k] = data[k]*gain;
  //memcpy(d,data,n_min*sizeof(float));
  if( n_min < n )
    memset(&(d[n_min]),0,sizeof(float)*(n-n_min));
  return n_min;
}

void wave_t::operator*=(double v)
{
  for( uint32_t k=0;k<n;k++)
    d[k] *= v;
}

uint32_t wave_t::copy_to(float* data,uint32_t cnt,float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;k++)
    data[k] = d[k]*gain;
  //memcpy(data,d,n_min*sizeof(float));
  if( n_min < cnt )
    memset(&(data[n_min]),0,sizeof(float)*(cnt-n_min));
  return n_min;
}

void wave_t::operator+=(const wave_t& o)
{
  for(uint32_t k=0;k<std::min(size(),o.size());k++)
    d[k]+=o[k];
}

float wave_t::rms() const
{
  float rv(0.0f);
  for(uint32_t k=0;k<size();k++)
    rv += d[k]*d[k];
  if( size() > 0 )
    rv /= size();
  return sqrt(rv);
}

float wave_t::spldb() const
{
  return 20.0*log10(rms())-SPLREF;
}

amb1wave_t::amb1wave_t(uint32_t chunksize)
  : w_(chunksize),x_(chunksize),y_(chunksize),z_(chunksize)
{
}

amb1wave_t::amb1wave_t(uint32_t chunksize,float* pw,float* px,float* py,float* pz)
  : w_(chunksize,pw),
    x_(chunksize,px),
    y_(chunksize,py),
    z_(chunksize,pz)
{
}

void amb1wave_t::clear()
{
  w_.clear();
  x_.clear();
  y_.clear();
  z_.clear();
}

void amb1wave_t::operator*=(double v)
{
  w_*=v;
  x_*=v;
  y_*=v;
  z_*=v;
}

sndfile_handle_t::sndfile_handle_t(const std::string& fname)
  : sfile(sf_open(TASCAR::env_expand(fname).c_str(),SFM_READ,&sf_inf))
{
  if( !sfile )
    throw TASCAR::ErrMsg("Unable to open sound file \""+fname+"\" for reading.");
}
    
sndfile_handle_t::~sndfile_handle_t()
{
  sf_close(sfile);
}

uint32_t sndfile_handle_t::readf_float( float* buf, uint32_t frames )
{
  return sf_readf_float( sfile, buf, frames );
}

sndfile_t::sndfile_t(const std::string& fname,uint32_t channel)
  : sndfile_handle_t(fname),
    wave_t(get_frames())
{
  uint32_t ch(get_channels());
  uint32_t N(get_frames());
  wave_t chbuf(N*ch);
  readf_float(chbuf.d,N);
  for(uint32_t k=0;k<N;k++)
    d[k] = chbuf[k*ch+channel];
}

void sndfile_t::add_chunk(int32_t chunk_time, int32_t start_time,float gain,wave_t& chunk)
{
  for(int32_t k=std::max(start_time,chunk_time);k < std::min(start_time+(int32_t)(size()),chunk_time+(int32_t)(chunk.size()));k++)
    chunk[k-chunk_time] += gain*d[k-start_time];
}

void wave_t::copy(const wave_t& src)
{
  memmove(d,src.d,std::min(n,src.n)*sizeof(float));
}

void wave_t::operator*=(const wave_t& o)
{
  for(unsigned int k=0;k<std::min(o.n,n);k++){
    d[k] *= o.d[k];
  }
}

void wave_t::append(const wave_t& src)
{
  if( src.n < n ){
    // copy from append_pos to end:
    uint32_t n1(std::min(n-append_pos,src.n));
    memmove(&(d[append_pos]),src.d,n1*sizeof(float));
    // if remainder then copy rest to beginning:
    if( n1 < src.n )
      memmove(d,&(src.d[n1]),(src.n-n1)*sizeof(float));
    append_pos = (append_pos+src.n) % n;
  }else{
    memmove(d,&(src.d[src.n-n]),n*sizeof(float));
    append_pos = 0;
  }
}

amb1rotator_t::amb1rotator_t(uint32_t chunksize)
  : amb1wave_t(chunksize),
    wxx(1), wxy(0), wxz(0), wyx(0), wyy(1), wyz(0), wzx(0), wzy(0), wzz(1), 
    dt(1.0/(double)chunksize)
{
}

amb1rotator_t& amb1rotator_t::rotate(const amb1wave_t& src,const zyx_euler_t& o,bool invert)
{
  float dxx;
  float dxy;
  float dxz;
  float dyx;
  float dyy;
  float dyz;
  float dzx;
  float dzy;
  float dzz;
  if( invert ){
    double cosy(cos(o.y));
    double siny(sin(-o.y));
    double cosz(cos(o.z));
    double sinz(sin(-o.z));
    double sinx(sin(-o.x));
    double cosx(cos(o.x));
    double sinxsiny(sinx*siny);
    double cosxsiny(cosx*siny);
    dxx = (cosz*cosy - wxx)*dt;
    dxy = (sinz*cosy - wxy)*dt;
    dxz = (siny - wxz)*dt;
    dyx = (-cosz*sinxsiny-sinz*cosx - wyx)*dt;
    dyy = (cosz*cosx-sinz*sinxsiny - wyy)*dt;
    dyz = (cosy*sinx - wyz)*dt;
    dzx = (-cosz*cosxsiny+sinz*sinx - wzx)*dt;
    dzy = (-cosz*sinx-sinz*cosxsiny - wzy)*dt;
    dzz = (cosy*cosx - wzz)*dt;
  }else{
    // 1, 0, 0
    // rot_z: cosz, sinz, 0
    // rot_y: cosy*cosz, sinz, siny*cosz
    // rot_x: cosy*cosz, cosx*sinz-sinx*siny*cosz, cosx*siny*cosz+sinx*sinz

    // 0, 1, 0
    // rot_z: -sinz, cosz, 0
    // rot_y: -cosy*sinz, cosz, -siny*sinz
    // rot_x: -cosy*sinz, cosx*cosz+sinx*siny*sinz, -cosx*siny*sinz + sinx*cosz

    // 0, 0, 1
    // rot_z: 0, 0, 1
    // rot_y: -siny, 0, cosy
    // rot_x: -siny, -sinx*cosy, cosx*cosy
    double cosy(cos(o.y));
    double cosz(cos(o.z));
    double cosx(cos(o.x));
    double sinz(sin(o.z));
    double siny(sin(o.y));
    double sinx(sin(o.x));
    double sinxsiny(sinx*siny);
    dxx = (cosy*cosz - wxx)*dt;
    dxy = (cosx*sinz-sinxsiny*cosz - wxy)*dt;
    dxz = (cosx*siny*cosz+sinx*sinz - wxz)*dt;
    dyx = (-cosy*sinz - wyx)*dt;
    dyy = (cosx*cosz+sinxsiny*sinz - wyy)*dt;
    dyz = (-cosx*siny*sinz+sinx*cosz - wyz)*dt;
    dzx = (-siny - wzx)*dt;
    dzy = (-sinx*cosy - wzy)*dt;
    dzz = (cosx*cosy - wzz)*dt;
  }
  w_.copy(src.w());
  float *p_src_x(src.x().d);
  float *p_src_y(src.y().d);
  float *p_src_z(src.z().d);
  float *p_x(x_.d);
  float *p_y(y_.d);
  float *p_z(z_.d);
  for(uint32_t k=0;k<w_.n;++k){
    *p_x = *p_src_x*(wxx+=dxx) + *p_src_y*(wxy+=dxy) + *p_src_z*(wxz+=dxz);
    *p_y = *p_src_x*(wyx+=dyx) + *p_src_y*(wyy+=dyy) + *p_src_z*(wyz+=dyz);
    *p_z = *p_src_x*(wzx+=dzx) + *p_src_y*(wzy+=dzy) + *p_src_z*(wzz+=dzz);
    ++p_x;
    ++p_y;
    ++p_z;
    ++p_src_x;
    ++p_src_y;
    ++p_src_z;
  }
  return *this;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
