#include "config.h"
#include "MainWindow4.h"
#include "Application4.h"
#include "Preferences4.h"

#include <algorithm>
#include <arpa/inet.h>
#include <ctime>
#include <glib/gi18n.h>

#include "iptux-core/CoreThread.h"
#include "iptux-core/Event.h"
#include "iptux-utils/output.h"

using namespace std;

namespace iptux {

// ─── GROUP MESSAGE PROTOCOL ──────────────────────────────────────────────────
// Messages prefixed with "[iptux-group:NAME:]" indicate a group message.
// When received, the UI creates/joins the named group automatically.
static const string GROUP_MSG_PREFIX = "[iptux-group:";
static const char GROUP_MSG_SEP = ']';

static string MakeGroupPrefix(const string& group_name) {
  return GROUP_MSG_PREFIX + group_name + GROUP_MSG_SEP + " ";
}
static bool ParseGroupMessage(const string& msg,
                              string& out_group,
                              string& out_body) {
  if (msg.rfind(GROUP_MSG_PREFIX, 0) != 0)
    return false;
  auto sep = msg.find(GROUP_MSG_SEP);
  if (sep == string::npos)
    return false;
  out_group =
      msg.substr(GROUP_MSG_PREFIX.size(), sep - GROUP_MSG_PREFIX.size());
  out_body = msg.substr(sep + 2);  // skip '] '
  return true;
}

// ─── Helper: styled text insert ──────────────────────────────────────────────
static void InsertTagged(GtkTextBuffer* buf,
                         GtkTextIter* iter,
                         const char* text,
                         int len,
                         const char* tag) {
  if (tag)
    gtk_text_buffer_insert_with_tags_by_name(buf, iter, text, len, tag,
                                             nullptr);
  else
    gtk_text_buffer_insert(buf, iter, text, len);
}

// File chooser data struct (file-scope to work in static callbacks)
struct MW4FileData {
  MainWindow4* win;
  ChatPane* pane;
};

// ─── Lifecycle
// ────────────────────────────────────────────────────────────────
static void mainWindowDestroy(gpointer data) {
  delete static_cast<MainWindow4*>(data);
}

MainWindow4::MainWindow4(Application4* app) : app_(app), window_(nullptr) {
  CreateWindow();
}
MainWindow4::~MainWindow4() {
  for (auto& [id, pane] : chat_panes_)
    delete pane;
}

void MainWindow4::Show() {
  gtk_window_present(GTK_WINDOW(window_));
}

// ─── Window construction
// ──────────────────────────────────────────────────────
void MainWindow4::CreateWindow() {
  window_ = ADW_APPLICATION_WINDOW(
      adw_application_window_new(GTK_APPLICATION(app_->getApp())));
  g_object_set_data_full(G_OBJECT(window_), "main-window4", this,
                         mainWindowDestroy);
  gtk_window_set_title(GTK_WINDOW(window_), _("Iptux Plus"));
  gtk_window_set_default_size(GTK_WINDOW(window_), 950, 660);
  gtk_window_set_icon_name(GTK_WINDOW(window_), "network-transmit-receive");

  auto split_view = adw_navigation_split_view_new();
  adw_navigation_split_view_set_min_sidebar_width(
      ADW_NAVIGATION_SPLIT_VIEW(split_view), 260);
  adw_navigation_split_view_set_max_sidebar_width(
      ADW_NAVIGATION_SPLIT_VIEW(split_view), 320);

  // ── Sidebar ──
  auto sidebar_tb = adw_toolbar_view_new();
  auto sidebar_hdr = adw_header_bar_new();
  adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(sidebar_hdr), FALSE);

  auto title_wid = adw_window_title_new(_("Iptux Plus"), nullptr);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(sidebar_hdr), title_wid);

  auto detect_btn = gtk_button_new_from_icon_name("system-search-symbolic");
  gtk_widget_set_tooltip_text(detect_btn, _("Detect Peers"));
  gtk_widget_add_css_class(detect_btn, "flat");
  adw_header_bar_pack_start(ADW_HEADER_BAR(sidebar_hdr), detect_btn);
  g_signal_connect(detect_btn, "clicked", G_CALLBACK(onDetect), this);

  auto group_btn = gtk_button_new_from_icon_name("list-add-symbolic");
  gtk_widget_set_tooltip_text(group_btn, _("New Group Chat"));
  gtk_widget_add_css_class(group_btn, "flat");
  adw_header_bar_pack_start(ADW_HEADER_BAR(sidebar_hdr), group_btn);
  g_signal_connect(group_btn, "clicked", G_CALLBACK(onNewGroupChat), this);

  auto refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
  gtk_widget_set_tooltip_text(refresh_btn, _("Refresh"));
  gtk_widget_add_css_class(refresh_btn, "flat");
  adw_header_bar_pack_end(ADW_HEADER_BAR(sidebar_hdr), refresh_btn);
  g_signal_connect(refresh_btn, "clicked", G_CALLBACK(onRefresh), this);

  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(sidebar_tb), sidebar_hdr);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(sidebar_tb), CreateSidebar());

  auto sidebar_page =
      adw_navigation_page_new(GTK_WIDGET(sidebar_tb), _("Contacts"));
  adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(split_view),
                                        sidebar_page);

  // ── Content area (stacked chat panes) ──
  auto content_tb = adw_toolbar_view_new();
  auto content_hdr = adw_header_bar_new();
  adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(content_hdr),
                                              FALSE);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(content_tb), content_hdr);

  content_stack_ = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(content_stack_),
                                GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration(GTK_STACK(content_stack_), 150);

  auto welcome = adw_status_page_new();
  adw_status_page_set_icon_name(ADW_STATUS_PAGE(welcome),
                                "chat-message-new-symbolic");
  adw_status_page_set_title(ADW_STATUS_PAGE(welcome),
                            _("Welcome to Iptux Plus"));
  adw_status_page_set_description(ADW_STATUS_PAGE(welcome),
                                  _("Select a contact to start chatting\nor "
                                    "create a group with the + button"));
  gtk_stack_add_named(GTK_STACK(content_stack_), GTK_WIDGET(welcome),
                      "welcome");

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(content_tb), content_stack_);

  auto content_page =
      adw_navigation_page_new(GTK_WIDGET(content_tb), _("Chat"));
  adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(split_view),
                                        content_page);

  adw_application_window_set_content(window_, GTK_WIDGET(split_view));
}

GtkWidget* MainWindow4::CreateSidebar() {
  auto box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  online_label_ = gtk_label_new(_("No peers online"));
  gtk_widget_add_css_class(online_label_, "caption");
  gtk_widget_add_css_class(online_label_, "dim-label");
  gtk_widget_set_margin_start(online_label_, 12);
  gtk_widget_set_margin_end(online_label_, 12);
  gtk_widget_set_margin_top(online_label_, 6);
  gtk_widget_set_margin_bottom(online_label_, 4);
  gtk_widget_set_halign(online_label_, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(box), online_label_);

  auto scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(box), scrolled);

  peer_list_box_ = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(peer_list_box_),
                                  GTK_SELECTION_SINGLE);
  gtk_widget_add_css_class(peer_list_box_, "navigation-sidebar");
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), peer_list_box_);
  g_signal_connect(peer_list_box_, "row-activated",
                   G_CALLBACK(onPeerRowActivated), this);

  auto placeholder = adw_status_page_new();
  adw_status_page_set_icon_name(ADW_STATUS_PAGE(placeholder),
                                "network-workgroup-symbolic");
  adw_status_page_set_title(ADW_STATUS_PAGE(placeholder), _("No Peers"));
  adw_status_page_set_description(ADW_STATUS_PAGE(placeholder),
                                  _("Press 🔍 to discover peers on LAN"));
  gtk_list_box_set_placeholder(GTK_LIST_BOX(peer_list_box_),
                               GTK_WIDGET(placeholder));

  return box;
}

GtkWidget* MainWindow4::CreatePeerRow(const PeerEntry& peer) {
  auto row = adw_action_row_new();
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), peer.name.c_str());

  string subtitle = peer.ip;
  if (!peer.group.empty())
    subtitle = peer.group + " · " + peer.ip;
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle.c_str());

  auto avatar = adw_avatar_new(36, peer.name.c_str(), TRUE);
  adw_action_row_add_prefix(ADW_ACTION_ROW(row), avatar);

  auto dot = gtk_image_new_from_icon_name("emblem-ok-symbolic");
  gtk_widget_add_css_class(dot, "success");
  adw_action_row_add_suffix(ADW_ACTION_ROW(row), dot);

  g_object_set_data_full(G_OBJECT(row), "peer-ip", g_strdup(peer.ip.c_str()),
                         g_free);
  g_object_set_data(G_OBJECT(row), "is-group", GINT_TO_POINTER(0));

  return GTK_WIDGET(row);
}

GtkWidget* MainWindow4::CreateGroupRow(const GroupEntry& group) {
  auto row = adw_action_row_new();
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), group.name.c_str());

  char subtitle[64];
  snprintf(subtitle, sizeof(subtitle), _("%d members"),
           (int)group.member_ips.size());
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);

  auto icon = gtk_image_new_from_icon_name("system-users-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 36);
  gtk_widget_add_css_class(icon, "accent");
  adw_action_row_add_prefix(ADW_ACTION_ROW(row), icon);

  g_object_set_data_full(G_OBJECT(row), "peer-ip", g_strdup(group.name.c_str()),
                         g_free);
  g_object_set_data(G_OBJECT(row), "is-group", GINT_TO_POINTER(1));

  return GTK_WIDGET(row);
}

// ─── Chat pane builder
// ────────────────────────────────────────────────────────
GtkWidget* MainWindow4::BuildChatPane(ChatPane* pane,
                                      const string& title,
                                      const string& subtitle) {
  auto vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Header info row
  auto top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start(top_box, 16);
  gtk_widget_set_margin_end(top_box, 16);
  gtk_widget_set_margin_top(top_box, 12);
  gtk_widget_set_margin_bottom(top_box, 12);

  auto avatar_size = pane->is_group ? 40 : 40;
  auto avatar = adw_avatar_new(avatar_size, title.c_str(), TRUE);
  gtk_box_append(GTK_BOX(top_box), avatar);

  auto info_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(info_vbox, TRUE);
  auto name_lbl = gtk_label_new(title.c_str());
  gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
  gtk_widget_add_css_class(name_lbl, "title-3");
  gtk_box_append(GTK_BOX(info_vbox), name_lbl);

  if (!subtitle.empty()) {
    auto sub_lbl = gtk_label_new(subtitle.c_str());
    gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(sub_lbl, "caption");
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_box_append(GTK_BOX(info_vbox), sub_lbl);
  }
  gtk_box_append(GTK_BOX(top_box), info_vbox);

  // Attach button in top bar
  auto attach_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_valign(attach_box, GTK_ALIGN_CENTER);
  auto attach_btn = gtk_button_new_from_icon_name("mail-attachment-symbolic");
  gtk_widget_set_tooltip_text(attach_btn, _("Send File"));
  gtk_widget_add_css_class(attach_btn, "flat");
  gtk_widget_add_css_class(attach_btn, "circular");
  g_object_set_data(G_OBJECT(attach_btn), "chat-pane", pane);
  g_signal_connect(attach_btn, "clicked", G_CALLBACK(onAttachClicked), pane);
  gtk_box_append(GTK_BOX(attach_box), attach_btn);
  gtk_box_append(GTK_BOX(top_box), attach_box);

  gtk_box_append(GTK_BOX(vbox), top_box);
  gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

  // Chat history
  auto scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(vbox), scrolled);

  pane->history_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(pane->history_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pane->history_view),
                              GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(pane->history_view), FALSE);
  gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(pane->history_view), 4);
  gtk_widget_set_margin_start(pane->history_view, 16);
  gtk_widget_set_margin_end(pane->history_view, 16);
  gtk_widget_set_margin_top(pane->history_view, 8);
  gtk_widget_set_margin_bottom(pane->history_view, 8);

  auto buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->history_view));
  gtk_text_buffer_create_tag(buf, "sender-self", "foreground", "#3584e4",
                             "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buf, "sender-other", "foreground", "#26a269",
                             "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buf, "timestamp", "foreground", "#77767b", "scale",
                             PANGO_SCALE_SMALL, nullptr);
  gtk_text_buffer_create_tag(buf, "message", nullptr);
  gtk_text_buffer_create_tag(buf, "file-share", "foreground", "#e66100",
                             "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  gtk_text_buffer_create_tag(buf, "system", "foreground", "#77767b",
                             "justification", GTK_JUSTIFY_CENTER, "scale", 0.85,
                             nullptr);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                pane->history_view);

  gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

  // Input row
  auto input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(input_box, 12);
  gtk_widget_set_margin_end(input_box, 12);
  gtk_widget_set_margin_top(input_box, 10);
  gtk_widget_set_margin_bottom(input_box, 10);

  pane->input_entry = gtk_entry_new();
  gtk_widget_set_hexpand(pane->input_entry, TRUE);
  gtk_entry_set_placeholder_text(
      GTK_ENTRY(pane->input_entry),
      pane->is_group ? _("Message the group…") : _("Message…"));
  g_signal_connect(pane->input_entry, "activate", G_CALLBACK(onEntryActivate),
                   pane);

  auto send_btn = gtk_button_new_from_icon_name("paper-plane-symbolic");
  gtk_widget_set_tooltip_text(send_btn, _("Send"));
  gtk_widget_add_css_class(send_btn, "suggested-action");
  gtk_widget_add_css_class(send_btn, "circular");
  g_signal_connect(send_btn, "clicked", G_CALLBACK(onSendClicked), pane);

  gtk_box_append(GTK_BOX(input_box), pane->input_entry);
  gtk_box_append(GTK_BOX(input_box), send_btn);
  gtk_box_append(GTK_BOX(vbox), input_box);

  pane->widget = vbox;
  return vbox;
}

void MainWindow4::AppendChatMessage(ChatPane* pane,
                                    const string& sender,
                                    const string& message,
                                    bool is_self) {
  auto buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->history_view));
  GtkTextIter end;

  if (sender.empty()) {
    gtk_text_buffer_get_end_iter(buf, &end);
    InsertTagged(buf, &end, "\n", -1, nullptr);
    gtk_text_buffer_get_end_iter(buf, &end);
    InsertTagged(buf, &end, message.c_str(), -1, "system");
    gtk_text_buffer_get_end_iter(buf, &end);
    InsertTagged(buf, &end, "\n", -1, nullptr);
    return;
  }

  time_t now = time(nullptr);
  struct tm* tm_info = localtime(&now);
  char time_buf[16];
  strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);

  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, sender.c_str(), -1,
               is_self ? "sender-self" : "sender-other");
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "  ", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, time_buf, -1, "timestamp");
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, message.c_str(), -1, "message");
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pane->history_view), &end, 0.0,
                               FALSE, 0.0, 1.0);
}

void MainWindow4::AppendChatFile(ChatPane* pane,
                                 const string& filename,
                                 bool is_self) {
  auto buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->history_view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  string msg = (is_self ? "📎 You shared: " : "📎 Shared file: ") + filename;
  InsertTagged(buf, &end, msg.c_str(), -1, "file-share");
  gtk_text_buffer_get_end_iter(buf, &end);
  InsertTagged(buf, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pane->history_view), &end, 0.0,
                               FALSE, 0.0, 1.0);
}

// ─── Chat pane management
// ─────────────────────────────────────────────────────
ChatPane* MainWindow4::OpenPeerChat(const string& ip) {
  if (chat_panes_.count(ip)) {
    SwitchToPane(ip);
    return chat_panes_[ip];
  }

  string name = peers_.count(ip) ? peers_[ip].name : ip;
  auto* pane = new ChatPane{ip, nullptr, nullptr, nullptr, false, {}, this};
  auto widget = BuildChatPane(pane, name, ip);
  chat_panes_[ip] = pane;
  gtk_stack_add_named(GTK_STACK(content_stack_), widget, ip.c_str());
  SwitchToPane(ip);
  return pane;
}

ChatPane* MainWindow4::OpenGroupChat(const string& group_name,
                                     const vector<string>& member_ips) {
  string id = "group:" + group_name;
  if (chat_panes_.count(id)) {
    SwitchToPane(id);
    return chat_panes_[id];
  }

  char subtitle[64];
  snprintf(subtitle, sizeof(subtitle), _("%d members"), (int)member_ips.size());
  auto* pane =
      new ChatPane{id, nullptr, nullptr, nullptr, true, member_ips, this};
  auto widget = BuildChatPane(pane, group_name, subtitle);
  chat_panes_[id] = pane;
  gtk_stack_add_named(GTK_STACK(content_stack_), widget, id.c_str());
  SwitchToPane(id);

  AppendChatMessage(pane, "", "— " + group_name + " created —", false);
  return pane;
}

void MainWindow4::RegisterGroup(const string& name,
                                const vector<string>& member_ips) {
  if (groups_.count(name))
    return;  // already exists

  groups_[name] = {name, member_ips};
  auto row = CreateGroupRow(groups_[name]);
  gtk_list_box_append(GTK_LIST_BOX(peer_list_box_), row);
}

void MainWindow4::SwitchToPane(const string& id) {
  gtk_stack_set_visible_child_name(GTK_STACK(content_stack_), id.c_str());
}

// ─── Sending messages
// ─────────────────────────────────────────────────────────
void MainWindow4::SendChatMessage(ChatPane* pane) {
  const char* text = gtk_editable_get_text(GTK_EDITABLE(pane->input_entry));
  if (!text || strlen(text) == 0)
    return;

  string message(text);
  gtk_editable_set_text(GTK_EDITABLE(pane->input_entry), "");

  string my_name = "Me";
  auto data = app_->getProgramData();
  if (data && !data->nickname.empty())
    my_name = data->nickname;

  AppendChatMessage(pane, my_name, message, true);

  auto cthrd = app_->getCoreThread();
  if (!cthrd)
    return;

  if (pane->is_group) {
    // Group message: prefix with group ID so recipients can identify it
    string gname = pane->id.substr(6);  // strip "group:"
    string prefixed = MakeGroupPrefix(gname) + message;
    // Keep shared_ptrs alive during the loop
    vector<PPalInfo> pal_refs;
    for (const auto& ip : pane->member_ips) {
      auto pal = cthrd->GetPal(ip);
      if (pal)
        pal_refs.push_back(pal);
    }
    for (auto& pal : pal_refs) {
      cthrd->SendMessage(pal, prefixed);
    }
  } else {
    auto pal = cthrd->GetPal(pane->id);
    if (pal)
      cthrd->SendMessage(pal, message);
  }
}

void MainWindow4::SendChatFile(ChatPane* pane) {
  auto chooser = gtk_file_dialog_new();
  gtk_file_dialog_set_title(GTK_FILE_DIALOG(chooser), _("Select File to Send"));
  gtk_file_dialog_set_modal(GTK_FILE_DIALOG(chooser), TRUE);

  auto* fd = new MW4FileData{this, pane};
  gtk_file_dialog_open(GTK_FILE_DIALOG(chooser), GTK_WINDOW(window_), nullptr,
                       onFileChosen, fd);
}

// ─── Peer list operations
// ──────────────────────────────────────────────────────
void MainWindow4::AddOrUpdatePeer(const string& ip,
                                  const string& name,
                                  const string& group,
                                  const string& host) {
  peers_[ip] = {ip, name, group, host, true};

  if (peer_rows_.count(ip)) {
    gtk_list_box_remove(GTK_LIST_BOX(peer_list_box_), peer_rows_[ip]);
  }

  auto& peer = peers_[ip];
  auto row = CreatePeerRow(peer);
  gtk_list_box_append(GTK_LIST_BOX(peer_list_box_), row);
  peer_rows_[ip] = row;

  RefreshOnlineCount();
}

void MainWindow4::RemovePeer(const string& ip) {
  if (peer_rows_.count(ip)) {
    gtk_list_box_remove(GTK_LIST_BOX(peer_list_box_), peer_rows_[ip]);
    peer_rows_.erase(ip);
  }
  peers_.erase(ip);
  RefreshOnlineCount();
}

void MainWindow4::DelItemFromPaltree(in_addr ipv4) {
  char buf[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &ipv4, buf, sizeof(buf));
  RemovePeer(string(buf));
}

void MainWindow4::ClearPeers() {
  for (auto& [ip, row] : peer_rows_)
    gtk_list_box_remove(GTK_LIST_BOX(peer_list_box_), row);
  peer_rows_.clear();
  peers_.clear();
  RefreshOnlineCount();
}

void MainWindow4::RefreshOnlineCount() {
  int n = (int)peers_.size();
  if (n == 0) {
    gtk_label_set_text(GTK_LABEL(online_label_), _("No peers online"));
  } else {
    char buf[64];
    snprintf(buf, sizeof(buf),
             n == 1 ? _("1 peer online") : _("%d peers online"), n);
    gtk_label_set_text(GTK_LABEL(online_label_), buf);
  }
}

// ─── Event processing
// ─────────────────────────────────────────────────────────
void MainWindow4::ProcessEvent(shared_ptr<const Event> event) {
  if (!event)
    return;
  auto type = event->getType();

  if (type == EventType::NEW_PAL_ONLINE || type == EventType::PAL_UPDATE) {
    const PalEvent* palEvt = dynamic_cast<const PalEvent*>(event.get());
    if (!palEvt)
      return;
    auto cthrd = app_->getCoreThread();
    if (!cthrd)
      return;
    auto pal = cthrd->GetPal(palEvt->GetPalKey());
    if (!pal)
      return;

    char buf[INET_ADDRSTRLEN];
    auto ipv4 = pal->ipv4();
    inet_ntop(AF_INET, &ipv4, buf, sizeof(buf));

    struct UpdateData {
      MainWindow4* win;
      string ip, name, group, host;
    };
    auto* d = new UpdateData{this, buf, pal->getName(), pal->getGroup(),
                             pal->getHost()};

    g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        [](gpointer p) -> gboolean {
          auto* d = static_cast<UpdateData*>(p);
          d->win->AddOrUpdatePeer(d->ip, d->name, d->group, d->host);
          delete d;
          return G_SOURCE_REMOVE;
        },
        d, nullptr);

  } else if (type == EventType::PAL_OFFLINE) {
    const PalEvent* palEvt = dynamic_cast<const PalEvent*>(event.get());
    if (!palEvt)
      return;
    in_addr ipv4 = palEvt->GetPalKey().GetIpv4();
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipv4, buf, sizeof(buf));
    string ip(buf);

    g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        [](gpointer p) -> gboolean {
          auto* d = static_cast<pair<MainWindow4*, string>*>(p);
          d->first->RemovePeer(d->second);
          delete d;
          return G_SOURCE_REMOVE;
        },
        new pair<MainWindow4*, string>(this, ip), nullptr);

  } else if (type == EventType::NEW_MESSAGE) {
    const auto* msgEvt = dynamic_cast<const NewMessageEvent*>(event.get());
    if (!msgEvt)
      return;

    const MsgPara& para = msgEvt->getMsgPara();
    auto pal = para.getPal();
    if (!pal)
      return;

    // Build sender IP
    char sender_ip_buf[INET_ADDRSTRLEN];
    auto ipv4 = pal->ipv4();
    inet_ntop(AF_INET, &ipv4, sender_ip_buf, sizeof(sender_ip_buf));
    string sender_ip(sender_ip_buf);
    string sender_name = pal->getName().empty() ? sender_ip : pal->getName();

    // Collect message text from chip data
    string full_msg;
    for (const auto& chip : para.dtlist) {
      if (chip.type == MessageContentType::STRING) {
        full_msg += chip.data;
      }
    }
    if (full_msg.empty())
      return;

    struct MsgData {
      MainWindow4* win;
      string sender_ip, sender_name, message;
    };
    auto* md = new MsgData{this, sender_ip, sender_name, full_msg};

    g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        [](gpointer p) -> gboolean {
          auto* md = static_cast<MsgData*>(p);
          auto* win = md->win;
          // Check if it's a group message
          string group_name, body;
          if (ParseGroupMessage(md->message, group_name, body)) {
            // Group message: open/create group pane
            if (!win->groups_.count(group_name)) {
              // We don't know this group yet — create it
              vector<string> members = {md->sender_ip};

              // See if this is an invite with a member list
              string invite_prefix = "📢 Group created — members: ";
              if (body.find(invite_prefix) == 0) {
                string ip_list = body.substr(invite_prefix.size());
                members.clear();
                size_t pos = 0;
                while ((pos = ip_list.find(',')) != string::npos) {
                  string ip = ip_list.substr(0, pos);
                  if (!ip.empty())
                    members.push_back(ip);
                  ip_list.erase(0, pos + 1);
                }
                if (!ip_list.empty())
                  members.push_back(ip_list);
              }
              win->RegisterGroup(group_name, members);
            } else {
              // Add sender to group if not present
              auto& mems = win->groups_[group_name].member_ips;
              if (find(mems.begin(), mems.end(), md->sender_ip) == mems.end()) {
                mems.push_back(md->sender_ip);
              }
            }
            string id = "group:" + group_name;
            if (!win->chat_panes_.count(id)) {
              win->OpenGroupChat(group_name,
                                 win->groups_[group_name].member_ips);
            }
            win->AppendChatMessage(win->chat_panes_[id], md->sender_name, body,
                                   false);
          } else {
            // Direct message
            const string& ip = md->sender_ip;
            if (!win->chat_panes_.count(ip)) {
              win->OpenPeerChat(ip);
            }
            win->AppendChatMessage(win->chat_panes_[ip], md->sender_name,
                                   md->message, false);
            // Switch to this chat if we're on welcome screen
            const char* cur = gtk_stack_get_visible_child_name(
                GTK_STACK(win->content_stack_));
            if (cur && string(cur) == "welcome") {
              win->SwitchToPane(ip);
            }
          }
          delete md;
          return G_SOURCE_REMOVE;
        },
        md, nullptr);
  }
}

// ─── Callbacks
// ────────────────────────────────────────────────────────────────
void MainWindow4::onRefresh(GtkButton* /*button*/, MainWindow4* self) {
  auto cthrd = self->app_->getCoreThread();
  if (cthrd) {
    cthrd->SendDetectPacket("255.255.255.255");
    auto data = cthrd->getProgramData();
    if (data) {
      for (const auto& seg : data->getNetSegments()) {
        for (uint64_t i = 0; i < seg.Count() && i < 256; ++i)
          cthrd->SendDetectPacket(seg.NthIp(i));
      }
    }
  }
}

void MainWindow4::onDetect(GtkButton* /*button*/, MainWindow4* self) {
  auto dialog = adw_alert_dialog_new(_("Detect Peers"), nullptr);
  adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
                            _("Enter an IP or subnet to scan:"));
  auto entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
                                 "192.168.1.1 or 255.255.255.255");
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel",
                                 _("Cancel"), "go", _("Detect"), nullptr);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "go",
                                           ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "go");

  struct DetectData {
    MainWindow4* win;
    GtkWidget* entry;
  };
  auto* dd = new DetectData{self, entry};
  g_signal_connect_data(
      dialog, "response",
      G_CALLBACK(+[](AdwAlertDialog* /*d*/, const char* resp, gpointer ptr) {
        auto* dd = static_cast<DetectData*>(ptr);
        if (g_strcmp0(resp, "go") == 0) {
          const char* txt = gtk_editable_get_text(GTK_EDITABLE(dd->entry));
          if (txt && strlen(txt) > 0) {
            auto cthrd = dd->win->app_->getCoreThread();
            if (cthrd)
              cthrd->SendDetectPacket(string(txt));
          }
        }
        delete dd;
      }),
      dd, [](gpointer, GClosure*) {}, G_CONNECT_AFTER);
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(self->window_));
}

void MainWindow4::onNewGroupChat(GtkButton* /*button*/, MainWindow4* self) {
  self->ShowNewGroupDialog();
}

void MainWindow4::ShowNewGroupDialog() {
  if (peers_.empty()) {
    auto d = adw_alert_dialog_new(_("No Peers"), nullptr);
    adw_alert_dialog_set_body(ADW_ALERT_DIALOG(d),
                              _("No peers online. Use Detect first."));
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(d), "ok", _("OK"), nullptr);
    adw_dialog_present(ADW_DIALOG(d), GTK_WIDGET(window_));
    return;
  }

  auto dialog = adw_alert_dialog_new(_("New Group Chat"), nullptr);
  adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
                            _("Name the group and select members:"));

  auto vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  auto name_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), _("Group name…"));
  gtk_box_append(GTK_BOX(vbox), name_entry);

  auto lbl = gtk_label_new(_("Members:"));
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_widget_add_css_class(lbl, "caption");
  gtk_box_append(GTK_BOX(vbox), lbl);

  auto scrolled = gtk_scrolled_window_new();
  gtk_widget_set_size_request(scrolled, -1, 160);
  auto members_box = gtk_list_box_new();
  gtk_widget_add_css_class(members_box, "boxed-list");
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), members_box);
  gtk_box_append(GTK_BOX(vbox), scrolled);

  vector<pair<string, GtkWidget*>> checks;
  for (auto& [ip, peer] : peers_) {
    auto row = gtk_list_box_row_new();
    auto hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(hbox, 10);
    gtk_widget_set_margin_end(hbox, 10);
    gtk_widget_set_margin_top(hbox, 7);
    gtk_widget_set_margin_bottom(hbox, 7);

    auto chk = gtk_check_button_new();
    gtk_box_append(GTK_BOX(hbox), chk);
    auto av = adw_avatar_new(28, peer.name.c_str(), TRUE);
    gtk_box_append(GTK_BOX(hbox), av);
    auto nlbl = gtk_label_new(peer.name.c_str());
    gtk_widget_set_hexpand(nlbl, TRUE);
    gtk_widget_set_halign(nlbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hbox), nlbl);
    auto iplbl = gtk_label_new(ip.c_str());
    gtk_widget_add_css_class(iplbl, "dim-label");
    gtk_widget_add_css_class(iplbl, "caption");
    gtk_box_append(GTK_BOX(hbox), iplbl);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
    gtk_list_box_append(GTK_LIST_BOX(members_box), row);
    g_object_set_data_full(G_OBJECT(chk), "peer-ip", g_strdup(ip.c_str()),
                           g_free);
    checks.push_back({ip, chk});
  }

  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), vbox);
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel",
                                 _("Cancel"), "create", _("Create"), nullptr);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "create",
                                           ADW_RESPONSE_SUGGESTED);

  struct GroupData {
    MainWindow4* win;
    GtkWidget* name_entry;
    vector<pair<string, GtkWidget*>> checks;
  };
  auto* gd = new GroupData{this, name_entry, checks};

  g_signal_connect_data(
      dialog, "response",
      G_CALLBACK(+[](AdwAlertDialog* /*d*/, const char* resp, gpointer ptr) {
        auto* gd = static_cast<GroupData*>(ptr);
        if (g_strcmp0(resp, "create") == 0) {
          const char* gname =
              gtk_editable_get_text(GTK_EDITABLE(gd->name_entry));
          string group_name = (gname && strlen(gname) > 0) ? gname : "Group";

          vector<string> members;
          for (auto& [ip, chk] : gd->checks) {
            if (gtk_check_button_get_active(GTK_CHECK_BUTTON(chk)))
              members.push_back(ip);
          }

          if (members.empty()) {
            delete gd;
            return;
          }

          auto* win = gd->win;
          win->RegisterGroup(group_name, members);
          auto* pane = win->OpenGroupChat(group_name, members);

          // Announce group to all members via protocol message
          auto cthrd = win->app_->getCoreThread();
          if (cthrd) {
            // Build member list string
            string member_list;
            for (const auto& ip : members) {
              if (!member_list.empty())
                member_list += ",";
              member_list += ip;
            }
            // Announce: "[iptux-group:NAME] You're invited. Members: ..."
            string announce = MakeGroupPrefix(group_name) +
                              "📢 Group created — members: " + member_list;

            // Keep shared_ptrs alive
            vector<PPalInfo> pal_refs;
            for (const auto& ip : members) {
              auto pal = cthrd->GetPal(ip);
              if (pal)
                pal_refs.push_back(pal);
            }
            for (auto& pal : pal_refs) {
              cthrd->SendMessage(pal, announce);
            }
          }
          (void)pane;
        }
        delete gd;
      }),
      gd, [](gpointer, GClosure*) {}, G_CONNECT_AFTER);

  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(window_));
}

void MainWindow4::onPeerRowActivated(GtkListBox* /*lb*/,
                                     GtkListBoxRow* row,
                                     MainWindow4* self) {
  const char* id =
      static_cast<const char*>(g_object_get_data(G_OBJECT(row), "peer-ip"));
  if (!id)
    return;

  int is_group = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "is-group"));
  if (is_group) {
    string group_name(id);
    if (self->groups_.count(group_name)) {
      self->OpenGroupChat(group_name, self->groups_[group_name].member_ips);
    }
  } else {
    self->OpenPeerChat(string(id));
  }
}

void MainWindow4::onSendClicked(GtkButton* /*button*/, ChatPane* pane) {
  if (pane->win)
    pane->win->SendChatMessage(pane);
}

void MainWindow4::onAttachClicked(GtkButton* /*button*/, ChatPane* pane) {
  if (pane->win)
    pane->win->SendChatFile(pane);
}

void MainWindow4::onEntryActivate(GtkEntry* /*entry*/, ChatPane* pane) {
  if (pane->win)
    pane->win->SendChatMessage(pane);
}

void MainWindow4::onFileChosen(GObject* source,
                               GAsyncResult* result,
                               gpointer data) {
  auto* fd = static_cast<MW4FileData*>(data);
  GError* error = nullptr;
  GFile* file =
      gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

  if (error) {
    if (error->code != GTK_DIALOG_ERROR_DISMISSED)
      LOG_WARN("File select error: %s", error->message);
    g_error_free(error);
    delete fd;
    return;
  }
  if (!file) {
    delete fd;
    return;
  }

  const char* path = g_file_get_path(file);
  if (!path) {
    g_object_unref(file);
    delete fd;
    return;
  }

  string filepath(path);
  g_object_unref(file);

  string fname = filepath.substr(filepath.find_last_of("/\\") + 1);

  // Show file in chat immediately
  fd->win->AppendChatFile(fd->pane, fname, true);

  // Send via CoreThread — keep shared_ptrs alive during send
  auto cthrd = fd->win->app_->getCoreThread();
  auto data_prog = fd->win->app_->getProgramData();
  if (cthrd && data_prog) {
    FileInfo fi;
    fi.filepath = g_strdup(filepath.c_str());
    fi.fileattr = FileAttr::REGULAR;
    fi.filesize = 0;
    fi.ensureFilesizeFilled();
    data_prog->AddShareFileInfo(fi);

    // Collect recipients, keeping shared_ptrs alive
    vector<PPalInfo> pal_refs;
    ChatPane* pane = fd->pane;

    if (pane->is_group) {
      for (const auto& ip : pane->member_ips) {
        auto pal = cthrd->GetPal(ip);
        if (pal)
          pal_refs.push_back(pal);
      }
    } else {
      auto pal = cthrd->GetPal(pane->id);
      if (pal)
        pal_refs.push_back(pal);
    }

    // Build raw-pointer list from live shared_ptrs
    if (!pal_refs.empty()) {
      vector<const PalInfo*> pals;
      pals.reserve(pal_refs.size());
      for (auto& p : pal_refs)
        pals.push_back(p.get());

      auto& shared = data_prog->GetSharedFileInfos();
      if (!shared.empty()) {
        vector<FileInfo*> files = {&shared.back()};
        cthrd->BcstFileInfoEntry(pals, files);
      }
    }
  }

  delete fd;
}

}  // namespace iptux
