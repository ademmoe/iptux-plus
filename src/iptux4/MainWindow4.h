#ifndef IPTUX4_MAINWINDOW4_H
#define IPTUX4_MAINWINDOW4_H

#include <adwaita.h>
#include <gtk/gtk.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "iptux-core/Event.h"
#include "iptux-core/Models.h"

namespace iptux {

class Application4;
class MainWindow4;

struct PeerEntry {
  std::string ip;
  std::string name;
  std::string group;
  std::string host;
  bool online = true;
};

struct GroupEntry {
  std::string name;
  std::vector<std::string> member_ips;
};

// In-window chat pane (embedded in main window's content area)
struct ChatPane {
  std::string id;           // peer IP or "group:GroupName"
  GtkWidget* widget;        // the whole chat box
  GtkWidget* history_view;  // GtkTextView
  GtkWidget* input_entry;   // GtkEntry
  bool is_group;
  std::vector<std::string> member_ips;  // for groups
  MainWindow4* win;                     // back-pointer to owning window
};

class MainWindow4 {
 public:
  explicit MainWindow4(Application4* app);
  ~MainWindow4();

  GtkWidget* getWindow() { return GTK_WIDGET(window_); }
  Application4* getApp() { return app_; }

  void Show();
  void ProcessEvent(std::shared_ptr<const Event> event);

  void AddOrUpdatePeer(const std::string& ip,
                       const std::string& name,
                       const std::string& group,
                       const std::string& host);
  void RemovePeer(const std::string& ip);
  void ClearPeers();

  // Compat stubs
  void UpdateItemToPaltree(in_addr ipv4) { (void)ipv4; }
  void AttachItemToPaltree(in_addr ipv4) { (void)ipv4; }
  void DelItemFromPaltree(in_addr ipv4);
  void ClearAllItemFromPaltree() { ClearPeers(); }
  bool PaltreeContainItem(in_addr ipv4) {
    (void)ipv4;
    return false;
  }

  // Open chat pane for a peer, returns existing or creates new
  ChatPane* OpenPeerChat(const std::string& ip);
  ChatPane* OpenGroupChat(const std::string& group_name,
                          const std::vector<std::string>& member_ips);
  void RegisterGroup(const std::string& name,
                     const std::vector<std::string>& member_ips);

 private:
  Application4* app_;
  AdwApplicationWindow* window_;

  GtkWidget* peer_list_box_ = nullptr;
  GtkWidget* online_label_ = nullptr;
  GtkWidget* content_stack_ = nullptr;

  std::map<std::string, GtkWidget*> peer_rows_;
  std::map<std::string, PeerEntry> peers_;
  std::map<std::string, GroupEntry> groups_;
  std::map<std::string, ChatPane*> chat_panes_;  // id -> pane

  void CreateWindow();
  GtkWidget* CreateSidebar();
  GtkWidget* CreatePeerRow(const PeerEntry& peer);
  GtkWidget* CreateGroupRow(const GroupEntry& group);
  GtkWidget* BuildChatPane(ChatPane* pane,
                           const std::string& title,
                           const std::string& subtitle);
  void AppendChatMessage(ChatPane* pane,
                         const std::string& sender,
                         const std::string& message,
                         bool is_self);
  void AppendChatFile(ChatPane* pane,
                      const std::string& filename,
                      bool is_self);
  void SendChatMessage(ChatPane* pane);
  void SendChatFile(ChatPane* pane);
  void SwitchToPane(const std::string& id);
  void RefreshOnlineCount();

  static void onDetect(GtkButton* button, MainWindow4* self);
  static void onNewGroupChat(GtkButton* button, MainWindow4* self);
  static void onRefresh(GtkButton* button, MainWindow4* self);
  static void onPeerRowActivated(GtkListBox* lb,
                                 GtkListBoxRow* row,
                                 MainWindow4* self);
  static void onSendClicked(GtkButton* button, ChatPane* pane);
  static void onAttachClicked(GtkButton* button, ChatPane* pane);
  static void onEntryActivate(GtkEntry* entry, ChatPane* pane);
  static void onFileChosen(GObject* source,
                           GAsyncResult* result,
                           gpointer data);

  void ShowNewGroupDialog();
};

}  // namespace iptux

#endif  // IPTUX4_MAINWINDOW4_H
