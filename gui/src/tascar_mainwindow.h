#ifndef TASCAR_MAINWINDOW_H
#define TASCAR_MAINWINDOW_H

#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include "scene_manager.h"
#include "viewport.h"
#include "gui_elements.h"
#include <gtksourceviewmm.h>
#include <webkit2/webkit2.h>

void error_message(const std::string& msg);

class tascar_window_t : public scene_manager_t, public Gtk::Window
{
public:
  tascar_window_t(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refGlade);
  virtual ~tascar_window_t();
  void load(const std::string& fname);
protected:
  Glib::RefPtr<Gtk::Builder> m_refBuilder;
  //Signal handlers:
  void on_menu_file_new();
  void on_menu_file_open();
  void on_menu_file_open_example();
  void on_menu_file_reload();
  void on_menu_file_exportcsv();
  void on_menu_file_exportcsvsounds();
  void on_menu_file_exportpdf();
  void on_menu_file_exportacmodel();
  void on_menu_file_close();
  void on_menu_file_quit();

  void on_menu_view_zoom_in();
  void on_menu_view_zoom_out();
  void on_menu_view_viewport_xy();
  void on_menu_view_viewport_xz();
  void on_menu_view_viewport_yz();
  void on_menu_view_viewport_xyz();
  void on_menu_view_viewport_rotz();
  void on_menu_view_viewport_rotzcw();
  void on_menu_view_viewport_setref();
  void on_menu_view_meter_rmspeak();
  void on_menu_view_meter_rms();
  void on_menu_view_meter_peak();
  void on_menu_view_meter_percentile();
  void on_menu_view_show_warnings();

  virtual bool draw_scene(const Cairo::RefPtr<Cairo::Context>& cr);
  bool on_timeout();
  bool on_timeout_blink();
  bool on_timeout_statusbar();
  void on_time_changed();

  void on_menu_transport_play();
  void on_menu_transport_stop();
  void on_menu_transport_rewind();
  void on_menu_transport_forward();
  void on_menu_transport_previous();
  void on_menu_transport_next();

  void on_menu_help_manual();
  void on_menu_help_bugreport();
  void on_menu_help_about();

  void set_scale(double s){draw.view.set_scale( s );};
  bool on_map_scroll(GdkEventScroll* e);
  bool on_map_clicked(GdkEventButton* e);

  void on_scene_selector_changed();
  void on_active_selector_changed();
  void on_active_track_changed();

  void reset_gui();
  void update_levelmeter_settings();
  void update_object_list();
  void update_selection_info();

  scene_draw_t draw;

  //Child widgets:
  //Gtk::VPaned vbox;
  //Gtk::Box m_Box;
  pthread_mutex_t mtx_draw;

  std::string srv_addr_;
  std::string srv_port_;
  std::string tascar_filename;
  std::string backend_flags_;
  bool nobackend_;

  int32_t selected_range;

  Gtk::Notebook* notebook;
  Gtk::DrawingArea* scene_map;
  //Gtk::EventBox* scene_map_events;
  Gtk::Statusbar* statusbar_main;
  Gtk::Scale* timeline;
  Gtk::TextView* text_warnings;
  //Gtk::TextView* text_source;
  Gtk::ScrolledWindow* scrolled_window_source;
  Gsv::View source_view;
  Glib::RefPtr<Gsv::LanguageManager> language_manager;
  Glib::RefPtr<Gsv::Buffer> source_buffer;
  Gtk::TextView* legal_view;
  Gtk::TextView* osc_vars;
  Gtk::Label* text_srv_addr;
  Gtk::Label* text_srv_port;
  source_panel_t* source_panel;
  Gtk::ComboBoxText* scene_selector;
  uint32_t selected_scene;
  Gtk::ComboBoxText* active_selector;
  TASCAR::Scene::object_t* active_object;
  Gtk::Label* active_type_label;
  Gtk::CheckButton* active_track;
  Gtk::Label* active_label_sourceline;
  Gtk::TextView* active_source_display;
  Gtk::ScrolledWindow* active_mixer;
  source_ctl_t* active_source_ctl;

  bool blink;

  sigc::connection con_draw;
  sigc::connection con_timeout;
  sigc::connection con_timeout_blink;
  sigc::connection con_timeout_statusbar;

  bool sessionloaded;
  bool sessionquit;

  WebKitWebView *news_view;
  Gtk::Box* news_box;
  Gtk::Widget* news_viewpp;
};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */