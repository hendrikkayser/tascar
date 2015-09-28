#include "session.h"
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include "errorhandling.h"
#include <dlfcn.h>
#include <fnmatch.h>
#include <locale.h>

static void module_error(std::string errmsg)
{
  throw TASCAR::ErrMsg("Submodule error: "+errmsg);
}

TASCAR::module_t::module_t(xmlpp::Element* xmlsrc,TASCAR::session_t* session)
  : xml_element_t(xmlsrc),
    lib(NULL),
    libdata(NULL),
    create_cb(NULL),
    destroy_cb(NULL),
    write_xml_cb(NULL),
    update_cb(NULL),
    configure_cb(NULL)
{
  get_attribute("name",name);
  std::string libname("tascar_");
  libname += name + ".so";
  lib = dlopen(libname.c_str(), RTLD_NOW );
  if( !lib )
    throw TASCAR::ErrMsg("Unable to open module \""+name+"\": "+dlerror());
  try{
    create_cb = (module_create_t)dlsym(lib,"tascar_create");
    if( !create_cb )
      throw TASCAR::ErrMsg("Unable to resolve \"tascar_create\" in module \""+name+"\".");
    destroy_cb = (module_destroy_t)dlsym(lib,"tascar_destroy");
    if( !destroy_cb )
      throw TASCAR::ErrMsg("Unable to resolve \"tascar_destroy\" in module \""+name+"\".");
    write_xml_cb = (module_write_xml_t)dlsym(lib,"tascar_write_xml");
    update_cb = (module_update_t)dlsym(lib,"tascar_update");
    configure_cb = (module_configure_t)dlsym(lib,"tascar_configure");
    libdata = create_cb(xmlsrc, session, module_error);
  }
  catch( ... ){
    dlclose(lib);
    throw;
  }
}

void TASCAR::module_t::write_xml()
{
  if( write_xml_cb )
    write_xml_cb(libdata,module_error);
}

void TASCAR::module_t::update(uint32_t frame,bool running)
{
  if( update_cb )
    update_cb(libdata,module_error,frame,running);
}

void TASCAR::module_t::configure(double srate,uint32_t fragsize)
{
  if( configure_cb )
    configure_cb(libdata,module_error,srate,fragsize);
}

TASCAR::module_t::~module_t()
{
  destroy_cb(libdata,module_error);
  dlclose(lib);
}

xmlpp::Element* assert_element(xmlpp::Element* e)
{
  if( !e )
    throw TASCAR::ErrMsg("NULL pointer element");
  return e;
}

const std::string& debug_str(const std::string& s)
{
  return s;
}

TASCAR::xml_doc_t::xml_doc_t()
  : doc(NULL)
{
  doc = new xmlpp::Document();
  doc->create_root_node("session");
}

TASCAR::xml_doc_t::xml_doc_t(const std::string& filename_or_data,load_type_t t)
  : doc(NULL)
{
  switch( t ){
  case LOAD_FILE :
    domp.parse_file(TASCAR::env_expand(filename_or_data));
    break;
  case LOAD_STRING :
    domp.parse_memory(filename_or_data);
    break;
  }
  doc = domp.get_document();
  if( !doc )
    throw TASCAR::ErrMsg("Unable to parse document.");
}

TASCAR::session_t::session_t()
  : xml_element_t(doc->get_root_node()),
    jackc_transport_t(jacknamer(e->get_attribute_value("name"),"session.")),
    osc_server_t(e->get_attribute_value("srv_addr"),e->get_attribute_value("srv_port")),
    name("tascar"),
    duration(60),
    loop(false),
    period_time(1.0/(double)srate),
    started_(false)
{
  // avoid problems with number format in xml file:
  setlocale(LC_ALL,"C");
  char c_respath[PATH_MAX];
  session_path = getcwd(c_respath,PATH_MAX);
  if( get_element_name() != "session" )
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got "+get_element_name()+".");
  read_xml();
  add_transport_methods();
  osc_server_t::activate();
}

TASCAR::session_t::session_t(const std::string& filename_or_data,load_type_t t,const std::string& path)
  : xml_doc_t(filename_or_data,t),
    xml_element_t(doc->get_root_node()),
    jackc_transport_t(jacknamer(e->get_attribute_value("name"),"session.")),
    osc_server_t(e->get_attribute_value("srv_addr"),e->get_attribute_value("srv_port")),
    name("tascar"),
    duration(60),
    loop(false),
    period_time(1.0/(double)srate),
    started_(false)
{
  // avoid problems with number format in xml file:
  setlocale(LC_ALL,"C");
  if( path.size() ){
    char c_fname[path.size()+1];
    char c_respath[PATH_MAX];
    memcpy(c_fname,path.c_str(),path.size()+1);
    session_path = realpath(dirname(c_fname),c_respath);
    if( chdir(session_path.c_str()) != 0 )
      std::cerr << "Unable to change directory\n";
  }else{
    char c_respath[PATH_MAX];
    session_path = getcwd(c_respath,PATH_MAX);
  }
  if( get_element_name() != "session" )
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got "+get_element_name()+".");
  read_xml();
  add_transport_methods();
  osc_server_t::activate();
}

void TASCAR::session_t::read_xml()
{
  get_attribute("name",name);
  get_attribute("duration",duration);
  get_attribute_bool("loop",loop);
  try{
    xmlpp::Node::NodeList subnodes = e->get_children();
    for(xmlpp::Node::NodeList::iterator sn=subnodes.begin();sn!=subnodes.end();++sn){
      xmlpp::Element* sne(dynamic_cast<xmlpp::Element*>(*sn));
      if( sne && ( sne->get_name() == "scene"))
        add_scene(sne);
      if( sne && ( sne->get_name() == "range"))
        add_range(sne);
      if( sne && ( sne->get_name() == "connect"))
        add_connection(sne);
      if( sne && ( sne->get_name() == "module"))
        add_module(sne);
    }
  }
  catch( ... ){
    for( std::vector<TASCAR::scene_player_t*>::iterator it=player.begin();it!=player.end();++it)
      delete (*it);
    for( std::vector<TASCAR::range_t*>::iterator it=ranges.begin();it!=ranges.end();++it)
      delete (*it);
    for( std::vector<TASCAR::connection_t*>::iterator it=connections.begin();it!=connections.end();++it)
      delete (*it);
    for( std::vector<TASCAR::module_t*>::iterator it=modules.begin();it!=modules.end();++it)
      delete (*it);
    throw;
  }
}


void TASCAR::session_t::save(const std::string& filename)
{
  write_xml();
  xml_doc_t::save(filename);
}

void TASCAR::session_t::write_xml()
{
  for( std::vector<TASCAR::scene_player_t*>::iterator it=player.begin();it!=player.end();++it)
    (*it)->write_xml();
  for( std::vector<TASCAR::range_t*>::iterator it=ranges.begin();it!=ranges.end();++it)
    (*it)->write_xml();
  for( std::vector<TASCAR::connection_t*>::iterator it=connections.begin();it!=connections.end();++it)
    (*it)->write_xml();
  for( std::vector<TASCAR::module_t*>::iterator it=modules.begin();it!=modules.end();++it)
    (*it)->write_xml();
}

void TASCAR::session_t::unload_modules()
{
  std::vector<TASCAR::module_t*> lmodules(modules);  
  modules.clear();
  for( std::vector<TASCAR::module_t*>::iterator it=lmodules.begin();it!=lmodules.end();++it)
    delete (*it);
}

TASCAR::session_t::~session_t()
{
  osc_server_t::deactivate();
  if( started_ )
    stop();
  for( std::vector<TASCAR::scene_player_t*>::iterator it=player.begin();it!=player.end();++it)
    delete (*it);
  for( std::vector<TASCAR::range_t*>::iterator it=ranges.begin();it!=ranges.end();++it)
    delete (*it);
  for( std::vector<TASCAR::connection_t*>::iterator it=connections.begin();it!=connections.end();++it)
    delete (*it);
  for( std::vector<TASCAR::module_t*>::iterator it=modules.begin();it!=modules.end();++it)
    delete (*it);
}

std::vector<std::string> TASCAR::session_t::get_render_output_ports() const
{
  std::vector<std::string> ports;
  for( std::vector<TASCAR::scene_player_t*>::const_iterator it=player.begin();it!=player.end();++it){
    std::vector<std::string> pports((*it)->get_output_ports());
    ports.insert(ports.end(),pports.begin(),pports.end());
  }
  return ports;
}

TASCAR::Scene::scene_t* TASCAR::session_t::add_scene(xmlpp::Element* src)
{
  if( !src )
    src = e->add_child("scene");
  player.push_back(new TASCAR::scene_player_t(src));
  player.back()->add_child_methods(this);
  return player.back();
}

TASCAR::range_t* TASCAR::session_t::add_range(xmlpp::Element* src)
{
  if( !src )
    src = e->add_child("range");
  ranges.push_back(new TASCAR::range_t(src));
  return ranges.back();
}

TASCAR::connection_t* TASCAR::session_t::add_connection(xmlpp::Element* src)
{
  if( !src )
    src = e->add_child("connect");
  connections.push_back(new TASCAR::connection_t(src));
  return connections.back();
}

TASCAR::module_t* TASCAR::session_t::add_module(xmlpp::Element* src)
{
  if( !src )
    src = e->add_child("module");
  modules.push_back(new TASCAR::module_t(src,this));
  return modules.back();
}

void TASCAR::session_t::start()
{
  add_output_port("sync_out");
  jackc_transport_t::activate();
  started_ = true;
  for(std::vector<TASCAR::scene_player_t*>::iterator ipl=player.begin();ipl!=player.end();++ipl)
    (*ipl)->start();
  for(std::vector<TASCAR::module_t*>::iterator imod=modules.begin();imod!=modules.end();++imod)
    (*imod)->configure(srate,fragsize);
  for(std::vector<TASCAR::connection_t*>::iterator icon=connections.begin();icon!=connections.end();++icon){
    try{
      connect((*icon)->src,(*icon)->dest);
    }
    catch(const std::exception& e){
      std::cerr << "Warning: " << e.what() << std::endl;
    }
  }
  for(std::vector<TASCAR::scene_player_t*>::iterator ipl=player.begin();ipl!=player.end();++ipl){
    try{
      connect(get_client_name()+":sync_out",(*ipl)->get_client_name()+":sync_in");
    }
    catch(const std::exception& e){
      std::cerr << "Warning: " << e.what() << std::endl;
    }
  }
}

int TASCAR::session_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame, bool tp_running)
{
  double t(period_time*(double)tp_frame);
  for(std::vector<TASCAR::module_t*>::iterator imod=modules.begin();imod!=modules.end();++imod)
    (*imod)->update(tp_frame,tp_running);
  if( loop && ( t >= duration ) )
    tp_locate(0u);
  return 0;
}

void TASCAR::session_t::stop()
{
  for(std::vector<TASCAR::scene_player_t*>::iterator ipl=player.begin();ipl!=player.end();++ipl)
    (*ipl)->stop();
  jackc_transport_t::deactivate();
  started_ = false;
}

void del_whitespace( xmlpp::Node* node )
{
    xmlpp::TextNode* nodeText = dynamic_cast<xmlpp::TextNode*>(node);
    if( nodeText && nodeText->is_white_space()){
	nodeText->get_parent()->remove_child(node);
    }else{
	xmlpp::Element* nodeElement = dynamic_cast<xmlpp::Element*>(node);
	if( nodeElement ){
	    xmlpp::Node::NodeList children = nodeElement->get_children();
	    for(xmlpp::Node::NodeList::iterator nita=children.begin();nita!=children.end();++nita){
		del_whitespace( *nita );
	    }
	}
    }
}

void TASCAR::xml_doc_t::save(const std::string& filename)
{
  if( doc ){
    del_whitespace( doc->get_root_node());
    doc->write_to_file_formatted(filename);
  }
}

void TASCAR::session_t::run(bool &b_quit)
{
  start();
  while( !b_quit ){
    usleep( 50000 );
    getchar();
    if( feof( stdin ) )
      b_quit = true;
  }
  stop();
}

uint32_t TASCAR::session_t::get_active_pointsources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_player_t*>::const_iterator it=player.begin();it!=player.end();++it)
    rv += (*it)->active_pointsources;
  return rv;
}

uint32_t TASCAR::session_t::get_total_pointsources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_player_t*>::const_iterator it=player.begin();it!=player.end();++it)
    rv += (*it)->total_pointsources;
  return rv;
}

uint32_t TASCAR::session_t::get_active_diffusesources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_player_t*>::const_iterator it=player.begin();it!=player.end();++it)
    rv += (*it)->active_diffusesources;
  return rv;
}

uint32_t TASCAR::session_t::get_total_diffusesources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_player_t*>::const_iterator it=player.begin();it!=player.end();++it)
    rv += (*it)->total_diffusesources;
  return rv;
}

TASCAR::range_t::range_t(xmlpp::Element* xmlsrc)
  : scene_node_base_t(xmlsrc),
    name(""),
    start(0),
    end(0)
{
  name = e->get_attribute_value("name");
  get_attribute("start",start);
  get_attribute("end",end);
}

void TASCAR::range_t::write_xml()
{
  e->set_attribute("name",name);
  set_attribute("start",start);
  set_attribute("end",end);
}

TASCAR::connection_t::connection_t(xmlpp::Element* xmlsrc)
  : xml_element_t(xmlsrc)
{
  get_attribute("src",src);
  get_attribute("dest",dest);
}

void TASCAR::connection_t::write_xml()
{
  set_attribute("src",src);
  set_attribute("dest",dest);
}

TASCAR::module_base_t::module_base_t(xmlpp::Element* xmlsrc,TASCAR::session_t* session_)
  : xml_element_t(xmlsrc),session(session_)
{
}

void TASCAR::module_base_t::write_xml()
{
}

void TASCAR::module_base_t::update(uint32_t frame,bool running)
{
}

void TASCAR::module_base_t::configure(double srate,uint32_t fragsize)
{
}

TASCAR::module_base_t::~module_base_t()
{
}

std::vector<TASCAR::named_object_t> TASCAR::session_t::find_objects(const std::string& pattern)
{
  std::vector<TASCAR::named_object_t> retv;
  for(std::vector<TASCAR::scene_player_t*>::iterator sit=player.begin();sit!=player.end();++sit){
    std::vector<TASCAR::Scene::object_t*> objs((*sit)->get_objects());
    std::string base("/"+(*sit)->name+"/");
    for(std::vector<TASCAR::Scene::object_t*>::iterator it=objs.begin();it!=objs.end();++it){
      std::string name(base+(*it)->get_name());
      if( fnmatch(pattern.c_str(),name.c_str(),FNM_PATHNAME) == 0 )
        retv.push_back(TASCAR::named_object_t(*it,name));
    }
  }
  return retv;
}

const std::string& TASCAR::session_t::get_session_path() const
{
  return session_path;
}

TASCAR::actor_module_t::actor_module_t(xmlpp::Element* xmlsrc,TASCAR::session_t* session,bool fail_on_empty)
  : module_base_t(xmlsrc,session)
{
  GET_ATTRIBUTE(actor);
  obj = session->find_objects(actor);
  if( fail_on_empty && obj.empty() )
    throw TASCAR::ErrMsg("No object matches actor pattern \""+actor+"\".");
}

void TASCAR::actor_module_t::write_xml()
{
  SET_ATTRIBUTE(actor);
}

void TASCAR::actor_module_t::set_location(const TASCAR::pos_t& l)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dlocation = l;
}

void TASCAR::actor_module_t::set_orientation(const TASCAR::zyx_euler_t& o)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dorientation = o;
}

void TASCAR::actor_module_t::add_location(const TASCAR::pos_t& l)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dlocation += l;
}

void TASCAR::actor_module_t::add_orientation(const TASCAR::zyx_euler_t& o)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dorientation += o;
}

namespace OSCSession {

  int _locate(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 1) && (types[0] == 'f') ){
      ((TASCAR::session_t*)user_data)->tp_locate(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _locatei(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 1) && (types[0] == 'i') ){
      ((TASCAR::session_t*)user_data)->tp_locate((uint32_t)(argv[0]->i));
      return 0;
    }
    return 1;
  }

  int _stop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 0) ){
      ((TASCAR::session_t*)user_data)->tp_stop();
      return 0;
    }
    return 1;
  }

  int _start(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 0) ){
      ((TASCAR::session_t*)user_data)->tp_start();
      return 0;
    }
    return 1;
  }

  int _unload_modules(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 0) ){
      ((TASCAR::session_t*)user_data)->unload_modules();
      return 0;
    }
    return 0;
  }

}


void TASCAR::session_t::add_transport_methods()
{
  osc_server_t::add_method("/transport/locate","f",OSCSession::_locate,this);
  osc_server_t::add_method("/transport/locatei","i",OSCSession::_locatei,this);
  osc_server_t::add_method("/transport/start","",OSCSession::_start,this);
  osc_server_t::add_method("/transport/stop","",OSCSession::_stop,this);
  osc_server_t::add_method("/transport/unload","",OSCSession::_unload_modules,this);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
