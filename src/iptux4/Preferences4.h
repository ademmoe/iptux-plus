#ifndef IPTUX4_PREFERENCES4_H
#define IPTUX4_PREFERENCES4_H

#include <adwaita.h>
#include <gtk/gtk.h>

namespace iptux {

class Application4;

class Preferences4 {
 public:
  static void ShowPreferences(Application4* app, GtkWindow* parent);

 private:
  Preferences4() = delete;
};

}  // namespace iptux

#endif  // IPTUX4_PREFERENCES4_H
