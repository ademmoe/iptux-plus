#ifndef IPTUX4_GROUPDIALOG4_H
#define IPTUX4_GROUPDIALOG4_H

#include <adwaita.h>
#include <gtk/gtk.h>
#include <map>
#include <string>
#include <vector>

namespace iptux {

class Application4;
class MainWindow4;

class GroupDialog4 {
 public:
  GroupDialog4(Application4* app,
               const std::string& group_name,
               const std::vector<std::string>& member_ips,
               MainWindow4* main_win);
  ~GroupDialog4();

  static GroupDialog4* GroupDialogEntry(
      Application4* app,
      const std::string& group_name,
      const std::vector<std::string>& member_ips,
      MainWindow4* main_win);

  GtkWindow* getWindow() { return GTK_WINDOW(window_); }

  // Receive a message into the chat history
  void ReceiveMessage(const std::string& sender, const std::string& message);

 private:
  Application4* app_;
  MainWindow4* main_win_;
  std::string group_name_;
  std::vector<std::string> member_ips_;

  AdwWindow* window_;
  GtkWidget* history_view_ = nullptr;
  GtkWidget* input_entry_ = nullptr;
  GtkWidget* member_list_ = nullptr;
  GtkWidget* send_btn_ = nullptr;
  GtkWidget* attach_btn_ = nullptr;

  void CreateWindow();
  void AppendMessage(const std::string& sender,
                     const std::string& message,
                     bool is_self);
  void SendMessage();
  void SendFile();
  void ShowMemberList();

  // Callbacks
  static void onSendClicked(GtkButton* button, GroupDialog4* self);
  static void onAttachClicked(GtkButton* button, GroupDialog4* self);
  static void onEntryActivate(GtkEntry* entry, GroupDialog4* self);
  static void onFileChosen(GObject* source,
                           GAsyncResult* result,
                           gpointer data);
};

}  // namespace iptux

#endif  // IPTUX4_GROUPDIALOG4_H
