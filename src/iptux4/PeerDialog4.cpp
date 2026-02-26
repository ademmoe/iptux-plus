#include "config.h"
#include "PeerDialog4.h"
#include "Application4.h"
#include "MainWindow4.h"

#include <ctime>
#include <glib/gi18n.h>

#include "iptux-core/CoreThread.h"
#include "iptux-utils/output.h"

using namespace std;

namespace iptux {

// Helper to insert tagged text without sentinel issues
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

// File-scope struct for file chooser callback
struct PD4FileData {
  PeerDialog4* self;
};

PeerDialog4::PeerDialog4(Application4* app,
                         const string& peer_name,
                         const string& peer_ip,
                         MainWindow4* main_win)
    : app_(app),
      main_win_(main_win),
      peer_name_(peer_name),
      peer_ip_(peer_ip),
      window_(nullptr) {
  (void)main_win_;
  CreateWindow();
}

PeerDialog4::~PeerDialog4() {}

void PeerDialog4::PeerDialogEntry(Application4* app,
                                  const string& peer_name,
                                  const string& peer_ip,
                                  MainWindow4* main_win) {
  auto dialog = new PeerDialog4(app, peer_name, peer_ip, main_win);
  gtk_window_present(dialog->getWindow());
}

void PeerDialog4::CreateWindow() {
  window_ = ADW_WINDOW(adw_window_new());

  string title = peer_name_.empty() ? peer_ip_ : peer_name_;
  gtk_window_set_title(GTK_WINDOW(window_), title.c_str());
  gtk_window_set_default_size(GTK_WINDOW(window_), 580, 500);
  gtk_window_set_transient_for(GTK_WINDOW(window_),
                               GTK_WINDOW(gtk_application_get_active_window(
                                   GTK_APPLICATION(app_->getApp()))));

  auto toolbar_view = adw_toolbar_view_new();
  auto header = adw_header_bar_new();

  auto title_wid = adw_window_title_new(title.c_str(), peer_ip_.c_str());
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title_wid);

  auto attach_btn = gtk_button_new_from_icon_name("mail-attachment-symbolic");
  gtk_widget_set_tooltip_text(attach_btn, _("Send File"));
  gtk_widget_add_css_class(attach_btn, "flat");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), attach_btn);
  g_signal_connect(attach_btn, "clicked", G_CALLBACK(onAttachClicked), this);

  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

  auto vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  auto scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(vbox), scrolled);

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

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), history_view_);

  gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

  auto input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(input_box, 12);
  gtk_widget_set_margin_end(input_box, 12);
  gtk_widget_set_margin_top(input_box, 10);
  gtk_widget_set_margin_bottom(input_box, 10);
  gtk_box_append(GTK_BOX(vbox), input_box);

  input_entry_ = gtk_entry_new();
  gtk_widget_set_hexpand(input_entry_, TRUE);
  gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry_),
                                 _("Type a message..."));
  gtk_box_append(GTK_BOX(input_box), input_entry_);
  g_signal_connect(input_entry_, "activate", G_CALLBACK(onEntryActivate), this);

  auto send_btn = gtk_button_new_with_label(_("Send"));
  gtk_widget_add_css_class(send_btn, "suggested-action");
  gtk_box_append(GTK_BOX(input_box), send_btn);
  g_signal_connect(send_btn, "clicked", G_CALLBACK(onSendClicked), this);

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), vbox);
  adw_window_set_content(window_, GTK_WIDGET(toolbar_view));
}

void PeerDialog4::AppendMessage(const string& sender,
                                const string& message,
                                bool is_self) {
  auto buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(history_view_));
  GtkTextIter end;

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

void PeerDialog4::ReceiveMessage(const string& message) {
  AppendMessage(peer_name_, message, false);
}

void PeerDialog4::SendMessage() {
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
    auto pal = cthrd->GetPal(peer_ip_);
    if (pal)
      cthrd->SendMessage(pal, message);
  }
}

void PeerDialog4::SendFile() {
  auto chooser = gtk_file_dialog_new();
  gtk_file_dialog_set_title(GTK_FILE_DIALOG(chooser), _("Select File to Send"));
  gtk_file_dialog_set_modal(GTK_FILE_DIALOG(chooser), TRUE);

  auto* fd = new PD4FileData{this};
  gtk_file_dialog_open(GTK_FILE_DIALOG(chooser), GTK_WINDOW(window_), nullptr,
                       onFileChosen, fd);
}

void PeerDialog4::onFileChosen(GObject* source,
                               GAsyncResult* result,
                               gpointer data) {
  auto* fd = static_cast<PD4FileData*>(data);
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

    auto pal = cthrd->GetPal(fd->self->peer_ip_);
    if (pal) {
      auto& shared = data_prog->GetSharedFileInfos();
      if (!shared.empty()) {
        vector<const PalInfo*> pals = {pal.get()};
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
    string file_msg = "📎 Sent file: " + fname;
    InsertTagged(buffer, &end, file_msg.c_str(), -1, "file-share");
    gtk_text_buffer_get_end_iter(buffer, &end);
    InsertTagged(buffer, &end, "\n", -1, nullptr);
  }

  delete fd;
}

void PeerDialog4::onSendClicked(GtkButton* /*button*/, PeerDialog4* self) {
  self->SendMessage();
}

void PeerDialog4::onAttachClicked(GtkButton* /*button*/, PeerDialog4* self) {
  self->SendFile();
}

void PeerDialog4::onEntryActivate(GtkEntry* /*entry*/, PeerDialog4* self) {
  self->SendMessage();
}

}  // namespace iptux
