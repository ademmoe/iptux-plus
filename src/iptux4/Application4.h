#ifndef IPTUX4_APPLICATION4_H
#define IPTUX4_APPLICATION4_H

#include <adwaita.h>
#include <gtk/gtk.h>
#include <memory>

#include "iptux-core/CoreThread.h"
#include "iptux-core/Event.h"
#include "iptux-core/IptuxConfig.h"
#include "iptux-core/Models.h"

namespace iptux {

class MainWindow4;

class Application4 {
 public:
  explicit Application4(std::shared_ptr<IptuxConfig> config);
  ~Application4();

  int run(int argc, char** argv);

  AdwApplication* getApp() { return app_; }
  std::shared_ptr<IptuxConfig> getConfig() { return config_; }
  std::shared_ptr<CoreThread> getCoreThread() { return cthrd_; }
  MainWindow4* getMainWindow() { return window_; }
  std::shared_ptr<ProgramData> getProgramData() { return data_; }

  void setTestMode(bool test) { test_mode_ = test; }
  bool isActivated() const { return activated_; }

 private:
  std::shared_ptr<IptuxConfig> config_;
  std::shared_ptr<ProgramData> data_;
  std::shared_ptr<CoreThread> cthrd_;

  AdwApplication* app_;
  MainWindow4* window_ = nullptr;
  bool started_{false};
  bool test_mode_{false};
  bool activated_{false};
  guint process_events_source_id_{0};

 public:
  void startup();
  void activate();

 private:
  void onEvent(std::shared_ptr<const Event> event);
  static void onStartup(Application4& self);
  static void onActivate(Application4& self);
  static void onQuit(void*, void*, Application4& self);
  static void onPreferences(void*, void*, Application4& self);
  static gboolean ProcessEvents(gpointer data);
};

}  // namespace iptux

#endif  // IPTUX4_APPLICATION4_H
