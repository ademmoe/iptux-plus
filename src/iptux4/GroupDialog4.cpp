#include "config.h"
#include "GroupDialog4.h"
#include "Application4.h"
#include "MainWindow4.h"

#include <ctime>
#include <glib/gi18n.h>

#include "iptux-core/CoreThread.h"
#include "iptux-core/Models.h"
#include "iptux-utils/output.h"

using namespace std;

namespace iptux {

// File-scope helper to insert text with a single tag (avoids sentinel issue)
static void InsertTagged(GtkTextBuffer* buf,
                         GtkTextIter* iter,
                         const char* text,
                         int len,
                         const char* tag) {
  if (tag) {
    gtk_text_buffer_insert_with_tags_by_name(buf, iter, text, len, tag,
                                             nullptr);
  } else {
    gtk_text_buffer_insert(buf, iter, text, len);
  }
}

// File-scope struct for file chooser callbacks
struct GD4FileData {
  GroupDialog4* self;
};

GroupDialog4::GroupDialog4(Application4* app,
                           const string& group_name,
                           const vector<string>& member_ips,
                           MainWindow4* main_win)
    : app_(app),
      main_win_(main_win),
      group_name_(group_name),
      member_ips_(member_ips),
      window_(nullptr) {
  (void)main_win_;
  CreateWindow();
}

GroupDialog4::~GroupDialog4() {}

GroupDialog4* GroupDialog4::GroupDialogEntry(Application4* app,
                                             const string& group_name,
                                             const vector<string>& member_ips,
                                             MainWindow4* main_win) {
  auto dialog = new GroupDialog4(app, group_name, member_ips, main_win);
  gtk_window_present(dialog->getWindow());
  return dialog;
}

void GroupDialog4::CreateWindow() {
  window_ = ADW_WINDOW(adw_window_new());

  char subtitle[64];
  snprintf(subtitle, sizeof(subtitle), _("%d members"),
           (int)member_ips_.size());

  gtk_window_set_title(GTK_WINDOW(window_), group_name_.c_str());
  gtk_window_set_default_size(GTK_WINDOW(window_), 720, 580);
  gtk_window_set_transient_for(GTK_WINDOW(window_),
                               GTK_WINDOW(gtk_application_get_active_window(
                                   GTK_APPLICATION(app_->getApp()))));

  // Root paned: chat (left) | members (right)
  auto paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 500);

  // === LEFT: Chat ===
  auto chat_toolbar = adw_toolbar_view_new();
  auto header = adw_header_bar_new();

  auto title_wid = adw_window_title_new(group_name_.c_str(), subtitle);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title_wid);

  auto members_btn = gtk_toggle_button_new();
  gtk_button_set_icon_name(GTK_BUTTON(members_btn), "system-users-symbolic");
  gtk_widget_set_tooltip_text(members_btn, _("Show Members"));
  gtk_widget_add_css_class(members_btn, "flat");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), members_btn);

  attach_btn_ = gtk_button_new_from_icon_name("mail-attachment-symbolic");
  gtk_widget_set_tooltip_text(attach_btn_, _("Send File to Group"));
  gtk_widget_add_css_class(attach_btn_, "flat");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), attach_btn_);
  g_signal_connect(attach_btn_, "clicked", G_CALLBACK(onAttachClicked), this);

  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(chat_toolbar), header);

  auto chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  auto banner = adw_banner_new(group_name_.c_str());
  adw_banner_set_button_label(ADW_BANNER(banner), nullptr);
  gtk_box_append(GTK_BOX(chat_vbox), GTK_WIDGET(banner));

  auto scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(chat_vbox), scrolled);

  history_view_ = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(history_view_), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(history_view_), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(history_view_), FALSE);
  gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(history_view_), 4);
  gtk_widget_set_margin_start(history_view_, 12);
  gtk_widget_set_margin_end(history_view_, 12);
  gtk_widget_set_margin_top(history_view_, 8);
  gtk_widget_set_margin_bottom(history_view_, 8);

  auto buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(history_view_));
  gtk_text_buffer_create_tag(buffer, "sender-self", "foreground", "#3584e4",
                             "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buffer, "sender-other", "foreground", "#26a269",
                             "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buffer, "timestamp", "foreground", "#77767b",
                             "scale", PANGO_SCALE_SMALL, nullptr);
  gtk_text_buffer_create_tag(buffer, "message", nullptr);
  gtk_text_buffer_create_tag(buffer, "file-share", "foreground", "#e66100",
                             "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  gtk_text_buffer_create_tag(buffer, "system", "foreground", "#77767b",
                             "justification", GTK_JUSTIFY_CENTER, "scale", 0.85,
                             nullptr);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), history_view_);

  string welcome_msg =
      "— " + group_name_ + " · " + to_string(member_ips_.size()) + " members —";
  ReceiveMessage("", welcome_msg);

  gtk_box_append(GTK_BOX(chat_vbox),
                 gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

  auto input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(input_box, 12);
  gtk_widget_set_margin_end(input_box, 12);
  gtk_widget_set_margin_top(input_box, 10);
  gtk_widget_set_margin_bottom(input_box, 10);
  gtk_box_append(GTK_BOX(chat_vbox), input_box);

  input_entry_ = gtk_entry_new();
  gtk_widget_set_hexpand(input_entry_, TRUE);
  gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry_),
                                 _("Type a message to the group..."));
  gtk_box_append(GTK_BOX(input_box), input_entry_);
  g_signal_connect(input_entry_, "activate", G_CALLBACK(onEntryActivate), this);

  send_btn_ = gtk_button_new_with_label(_("Send"));
  gtk_widget_add_css_class(send_btn_, "suggested-action");
  gtk_box_append(GTK_BOX(input_box), send_btn_);
  g_signal_connect(send_btn_, "clicked", G_CALLBACK(onSendClicked), this);

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(chat_toolbar), chat_vbox);
  gtk_paned_set_start_child(GTK_PANED(paned), GTK_WIDGET(chat_toolbar));

  // === RIGHT: Members ===
  auto members_toolbar = adw_toolbar_view_new();
  auto members_header = adw_header_bar_new();
  auto members_title = adw_window_title_new(_("Members"), subtitle);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(members_header),
                                  members_title);
  adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(members_header),
                                              FALSE);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(members_toolbar),
                               members_header);

  auto members_scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(members_scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(members_scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  member_list_ = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(member_list_),
                                  GTK_SELECTION_NONE);
  gtk_widget_add_css_class(member_list_, "boxed-list");
  gtk_widget_set_margin_start(member_list_, 12);
  gtk_widget_set_margin_end(member_list_, 12);
  gtk_widget_set_margin_top(member_list_, 8);
  gtk_widget_set_margin_bottom(member_list_, 8);

  auto cthrd = app_->getCoreThread();
  for (const auto& ip : member_ips_) {
    string name = ip;
    if (cthrd) {
      auto pal = cthrd->GetPal(ip);
      if (pal)
        name = pal->getName();
    }
    auto row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), name.c_str());
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), ip.c_str());
    auto av = adw_avatar_new(32, name.c_str(), TRUE);
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), av);
    auto dot = gtk_image_new_from_icon_name("emblem-ok-symbolic");
    gtk_widget_add_css_class(dot, "success");
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), dot);
    gtk_list_box_append(GTK_LIST_BOX(member_list_), GTK_WIDGET(row));
  }

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(members_scrolled),
                                member_list_);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(members_toolbar),
                               members_scrolled);

  gtk_paned_set_end_child(GTK_PANED(paned), GTK_WIDGET(members_toolbar));

  g_signal_connect(members_btn, "toggled",
                   G_CALLBACK(+[](GtkToggleButton* btn, gpointer data) {
                     GtkPaned* p = GTK_PANED(data);
                     GtkWidget* end = gtk_paned_get_end_child(p);
                     gtk_widget_set_visible(end,
                                            gtk_toggle_button_get_active(btn));
                   }),
                   paned);

  adw_window_set_content(window_, GTK_WIDGET(paned));
}

void GroupDialog4::AppendMessage(const string& sender,
                                 const string& message,
                                 bool is_self) {
  auto buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(history_view_));
  GtkTextIter end;

  if (sender.empty()) {
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, "\n", -1, nullptr);
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, message.c_str(), -1, "system");
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, "\n", -1, nullptr);
    return;
  }

  time_t now = time(nullptr);
  struct tm* tm_info = localtime(&now);
  char time_buf[16];
  strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);

  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buffer, &end);

  const char* stag = is_self ? "sender-self" : "sender-other";
  InsertTagged(buffer, &end, sender.c_str(), -1, stag);
  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, "  ", -1, nullptr);
  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, time_buf, -1, "timestamp");
  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, message.c_str(), -1, "message");
  gtk_text_buffer_get_end_iter(buffer, &end);
  InsertTagged(buffer, &end, "\n", -1, nullptr);
  gtk_text_buffer_get_end_iter(buffer, &end);

  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(history_view_), &end, 0.0, FALSE,
                               0.0, 1.0);
}

void GroupDialog4::ReceiveMessage(const string& sender, const string& message) {
  AppendMessage(sender, message, false);
}

void GroupDialog4::SendMessage() {
  const char* text = gtk_editable_get_text(GTK_EDITABLE(input_entry_));
  if (!text || strlen(text) == 0)
    return;

  string message(text);
  gtk_editable_set_text(GTK_EDITABLE(input_entry_), "");

  string my_name = "Me";
  auto data = app_->getProgramData();
  if (data && !data->nickname.empty())
    my_name = data->nickname;

  AppendMessage(my_name, message, true);

  auto cthrd = app_->getCoreThread();
  if (cthrd) {
    for (const auto& ip : member_ips_) {
      auto pal = cthrd->GetPal(ip);
      if (pal)
        cthrd->SendMessage(pal, message);
    }
  }
}

void GroupDialog4::SendFile() {
  auto chooser = gtk_file_dialog_new();
  gtk_file_dialog_set_title(GTK_FILE_DIALOG(chooser),
                            _("Select File to Share"));
  gtk_file_dialog_set_modal(GTK_FILE_DIALOG(chooser), TRUE);

  auto* fd = new GD4FileData{this};
  gtk_file_dialog_open(GTK_FILE_DIALOG(chooser), GTK_WINDOW(window_), nullptr,
                       onFileChosen, fd);
}

void GroupDialog4::onFileChosen(GObject* source,
                                GAsyncResult* result,
                                gpointer data) {
  auto* fd = static_cast<GD4FileData*>(data);
  GError* error = nullptr;
  GFile* file =
      gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

  if (error) {
    if (error->code != GTK_DIALOG_ERROR_DISMISSED) {
      LOG_WARN("File choose error: %s", error->message);
    }
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

  auto cthrd = fd->self->app_->getCoreThread();
  auto data_prog = fd->self->app_->getProgramData();

  if (cthrd && data_prog) {
    FileInfo fi;
    fi.filepath = g_strdup(filepath.c_str());
    fi.fileattr = FileAttr::REGULAR;
    fi.filesize = 0;
    fi.ensureFilesizeFilled();
    data_prog->AddShareFileInfo(fi);

    vector<const PalInfo*> pals;
    for (const auto& ip : fd->self->member_ips_) {
      auto pal = cthrd->GetPal(ip);
      if (pal)
        pals.push_back(pal.get());
    }

    if (!pals.empty()) {
      auto& shared = data_prog->GetSharedFileInfos();
      if (!shared.empty()) {
        vector<FileInfo*> files = {&shared.back()};
        cthrd->BcstFileInfoEntry(pals, files);
      }
    }

    string fname = filepath.substr(filepath.find_last_of("/\\") + 1);
    auto buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(fd->self->history_view_));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, "\n", -1, nullptr);
    gtk_text_buffer_get_end_iter(buffer, &end);
    string file_msg = "📎 Shared file: " + fname;
    InsertTagged(buffer, &end, file_msg.c_str(), -1, "file-share");
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, "\n", -1, nullptr);
  }

  delete fd;
}

void GroupDialog4::onSendClicked(GtkButton* /*button*/, GroupDialog4* self) {
  self->SendMessage();
}

void GroupDialog4::onAttachClicked(GtkButton* /*button*/, GroupDialog4* self) {
  self->SendFile();
}

void GroupDialog4::onEntryActivate(GtkEntry* /*entry*/, GroupDialog4* self) {
  self->SendMessage();
}

}  // namespace iptux
