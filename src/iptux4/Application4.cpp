#include "config.h"
#include "Application4.h"

#include <csignal>
#include <glib/gi18n.h>

#include "iptux-core/CoreThread.h"
#include "iptux-core/Event.h"
#include "iptux-utils/output.h"
#include "iptux4/MainWindow4.h"
#include "iptux4/Preferences4.h"

using namespace std;

namespace iptux {

static void appDestroy(gpointer user_data) {
  LOG_TRACE();
  Application4* app = (Application4*)user_data;
  delete app;
}

Application4::Application4(shared_ptr<IptuxConfig> config)
    : config_(config), data_(nullptr), window_(nullptr) {
  auto application_id = config_->GetString("debug_application_id",
                                           "io.github.iptux_src.iptux_plus");

  data_ = make_shared<ProgramData>(config_);
  app_ = adw_application_new(application_id.c_str(), G_APPLICATION_FLAGS_NONE);
  g_object_set_data_full(G_OBJECT(app_), "application4", this, appDestroy);
  g_signal_connect_swapped(app_, "startup", G_CALLBACK(onStartup), this);
  g_signal_connect_swapped(app_, "activate", G_CALLBACK(onActivate), this);
}

Application4::~Application4() {
  if (process_events_source_id_) {
    g_source_remove(process_events_source_id_);
  }
  if (cthrd_) {
    cthrd_->stop();
  }
}

int Application4::run(int argc, char** argv) {
  return g_application_run(G_APPLICATION(app_), argc, argv);
}

void Application4::startup() {
  Application4::onStartup(*this);
}

void Application4::activate() {
  Application4::onActivate(*this);
}

void Application4::onStartup(Application4& self) {
  self.cthrd_ = make_shared<CoreThread>(self.data_);
  self.window_ = new MainWindow4(&self);

  GActionEntry app_entries[] = {
      {"quit",
       [](GSimpleAction*, GVariant*, gpointer ptr) {
         auto& s = *static_cast<Application4*>(ptr);
         onQuit(nullptr, nullptr, s);
       },
       nullptr,
       nullptr,
       nullptr,
       {0}},
      {"preferences",
       [](GSimpleAction*, GVariant*, gpointer ptr) {
         auto& s = *static_cast<Application4*>(ptr);
         onPreferences(nullptr, nullptr, s);
       },
       nullptr,
       nullptr,
       nullptr,
       {0}},
  };
  g_action_map_add_action_entries(G_ACTION_MAP(self.app_), app_entries,
                                  G_N_ELEMENTS(app_entries), &self);
}

void Application4::onActivate(Application4& self) {
  if (self.started_) {
    gtk_window_present(GTK_WINDOW(self.window_->getWindow()));
    return;
  }
  self.started_ = true;

  if (!self.cthrd_->start()) {
    LOG_ERROR("Start core thread failed");

    // Show an error dialog instead of silently quitting
    auto err = adw_alert_dialog_new(_("Failed to Start"), nullptr);
    auto err_msg = string(_("Could not bind to port ")) +
                   to_string(self.data_->port()) +
                   _(". Another instance of Iptux may already be running.\n\n"
                     "Try: iptux-plus --port <other_port>   if supported, or "
                     "close the other instance first.");
    adw_alert_dialog_set_body(ADW_ALERT_DIALOG(err), err_msg.c_str());
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(err), "quit", _("Quit"),
                                   nullptr);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(err), "quit");

    g_signal_connect(
        err, "response",
        G_CALLBACK(+[](AdwAlertDialog*, const char*, gpointer ptr) {
          g_application_quit(G_APPLICATION(ptr));
        }),
        self.app_);
    adw_dialog_present(ADW_DIALOG(err), GTK_WIDGET(self.window_->getWindow()));
    gtk_window_present(GTK_WINDOW(self.window_->getWindow()));
    return;
  }

  signal(SIGPIPE, SIG_IGN);
  self.process_events_source_id_ =
      g_timeout_add(100, Application4::ProcessEvents, &self);
  self.activated_ = true;

  gtk_window_present(GTK_WINDOW(self.window_->getWindow()));
}

void Application4::onQuit(void*, void*, Application4& self) {
  g_application_quit(G_APPLICATION(self.app_));
}

void Application4::onPreferences(void*, void*, Application4& self) {
  Preferences4::ShowPreferences(&self, GTK_WINDOW(self.window_->getWindow()));
}

void Application4::onEvent(shared_ptr<const Event> event) {
  if (!event)
    return;
  if (window_) {
    window_->ProcessEvent(event);
  }
}

gboolean Application4::ProcessEvents(gpointer data) {
  auto self = static_cast<Application4*>(data);
  bool had_event = false;

  // Process up to 10 events per tick to avoid blocking UI
  for (int i = 0; i < 10 && self->cthrd_ && self->cthrd_->HasEvent(); ++i) {
    auto e = self->cthrd_->PopEvent();
    self->onEvent(e);
    had_event = true;
  }

  (void)had_event;
  self->process_events_source_id_ =
      g_timeout_add(100, Application4::ProcessEvents, data);
  return G_SOURCE_REMOVE;
}

}  // namespace iptux
