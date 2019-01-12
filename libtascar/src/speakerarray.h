#ifndef SPEAKERARRAY_H
#define SPEAKERARRAY_H

#include "xmlconfig.h"
#include "filterclass.h"
#include "delayline.h"
#include "ola.h"

namespace TASCAR {

  /**
     \brief Description of a single loudspeaker in a speaker array.
   */
  class spk_descriptor_t : public xml_element_t, public pos_t {
  public:
    // if you add members please update copy constructor!
    spk_descriptor_t(xmlpp::Element*);
    spk_descriptor_t(const spk_descriptor_t&);
    virtual ~spk_descriptor_t();
    double get_rel_azim(double az_src) const;
    double get_cos_adist(pos_t src_unit) const;
    double az;
    double el;
    double r;
    std::string label;
    std::string connect;
    std::vector<double> compA;
    std::vector<double> compB;
    // derived parameters:
    pos_t unitvector;
    double gain;
    double dr;
    // decoder matrix:
    void update_foa_decoder(float gain, double xyzgain);
    float d_w;
    float d_x;
    float d_y;
    float d_z;
    float densityweight;
    TASCAR::filter_t* comp;
  };

  /**
     \brief Loudspeaker array.
   */
  class spk_array_t : public xml_element_t, public std::vector<spk_descriptor_t>, public audiostates_t {
  public:
    spk_array_t(xmlpp::Element*, const std::string& elementname_="speaker");
    ~spk_array_t();
  private:
    spk_array_t(const spk_array_t&);
  public:
    double get_rmax() const { return rmax;};
    double get_rmin() const { return rmin;};
    class didx_t {
    public:
      didx_t() : d(0),idx(0) {};
      double d;
      uint32_t idx;
    };
    const std::vector<didx_t>& sort_distance(const pos_t& psrc);
    void render_diffuse(std::vector<TASCAR::wave_t>& output);
    void add_diffuse_sound_field( const TASCAR::amb1wave_t& diff );
    void prepare( chunk_cfg_t& );
  private:
    void import_file(const std::string& fname);
    void read_xml(xmlpp::Element* elem);
    TASCAR::amb1wave_t* diffuse_field_accumulator;
    TASCAR::wave_t* diffuse_render_buffer;
    double rmax;
    double rmin;
    std::vector<didx_t> didx;
    double xyzgain;
    std::string elementname;
    xmlpp::DomParser domp;
  public:
    std::vector<std::string> connections;
    std::vector<TASCAR::static_delay_t> delaycomp;
    double mean_rotation;
  private:
    std::vector<TASCAR::overlap_save_t> decorrflt;
  public:
    double decorr_length;
    bool decorr;
    bool densitycorr;
  };

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */