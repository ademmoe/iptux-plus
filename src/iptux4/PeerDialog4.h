#ifndef IPTUX4_PEERDIALOG4_H
#define IPTUX4_PEERDIALOG4_H

#include <adwaita.h>
#include <gtk/gtk.h>
#include <string>

namespace iptux {

class Application4;
class MainWindow4;

class PeerDialog4 {
 public:
  PeerDialog4(Application4* app,
              const std::string& peer_name,
              const std::string& peer_ip,
              MainWindow4* main_win);
  ~PeerDialog4();

  static void PeerDialogEntry(Application4* app,
                              const std::string& peer_name,
                              const std::string& peer_ip,
                              MainWindow4* main_win);

  GtkWindow* getWindow() { return GTK_WINDOW(window_); }

  // Called when a message is received from this peer
  void ReceiveMessage(const std::string& message);

 private:
  Application4* app_;
  MainWindow4* main_win_;
  std::string peer_name_;
  std::string peer_ip_;

  AdwWindow* window_;
  GtkWidget* history_view_ = nullptr;
  GtkWidget* input_entry_ = nullptr;

  void CreateWindow();
  void AppendMessage(const std::string& sender,
                     const std::string& message,
                     bool is_self);
  void SendMessage();
  void SendFile();

  static void onSendClicked(GtkButton* button, PeerDialog4* self);
  static void onAttachClicked(GtkButton* button, PeerDialog4* self);
  static void onEntryActivate(GtkEntry* entry, PeerDialog4* self);
  static void onFileChosen(GObject* source,
                           GAsyncResult* result,
                           gpointer data);
};

}  // namespace iptux

#endif  // IPTUX4_PEERDIALOG4_H
