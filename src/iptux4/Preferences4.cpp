#include "config.h"
#include "Preferences4.h"
#include "Application4.h"

#include <glib/gi18n.h>

namespace iptux {

void Preferences4::ShowPreferences(Application4* app, GtkWindow* parent) {
  // AdwPreferencesDialog is the modern API in libadwaita 1.5+
  auto dialog = ADW_PREFERENCES_DIALOG(adw_preferences_dialog_new());
  adw_dialog_set_title(ADW_DIALOG(dialog), _("Preferences"));

  // Personal page
  auto personal_page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(personal_page, _("Personal"));
  adw_preferences_page_set_icon_name(personal_page, "person-symbolic");

  auto personal_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(personal_group, _("User Information"));
  adw_preferences_page_add(personal_page, personal_group);

  auto nickname_row = ADW_ENTRY_ROW(adw_entry_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(nickname_row),
                                _("Nickname"));
  auto config = app->getConfig();
  if (config) {
    gtk_editable_set_text(GTK_EDITABLE(nickname_row),
                          config->GetString("nick_name").c_str());
  }
  adw_preferences_group_add(personal_group, GTK_WIDGET(nickname_row));

  adw_preferences_dialog_add(dialog, personal_page);

  // Network page
  auto network_page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(network_page, _("Network"));
  adw_preferences_page_set_icon_name(network_page, "network-wireless-symbolic");

  auto network_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(network_group, _("Network Settings"));
  adw_preferences_page_add(network_page, network_group);

  auto port_row = ADW_ENTRY_ROW(adw_entry_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(port_row), _("Port"));
  gtk_editable_set_text(GTK_EDITABLE(port_row), "2425");
  adw_preferences_group_add(network_group, GTK_WIDGET(port_row));

  adw_preferences_dialog_add(dialog, network_page);

  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(parent));
}

}  // namespace iptux
